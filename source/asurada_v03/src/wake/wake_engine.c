// 호출어 엔진 (C). 설명은 wake_engine.h
#include "wake_engine.h"

#include <string.h>

#include "esp_timer.h"
#include "frontend.h"
#include "frontend_util.h"
#include "tflm_runner.h"

#define SAMPLE_RATE 16000
#define FEATURE_SIZE 40
#define SLIDING_WINDOW 5                 // 최근 몇 번의 추론을 평균할지
#define COOL_OFF_SLICES 100              // 감지 뒤 쉬는 시간: 특징값 100줄 = 1초
#ifndef TENSOR_ARENA_SIZE
#define TENSOR_ARENA_SIZE (48 * 1024)    // 실제 사용량은 30 KB 쯤 (wake_engine_arena_used 로 확인)
#endif

static struct FrontendConfig s_fe_cfg;
static struct FrontendState s_fe;
static int s_stride;
static int s_step;                       // 지금 입력 버퍼에 몇 줄 찼는지
static uint8_t s_recent[SLIDING_WINDOW];
static int s_recent_idx;
static int s_ignore = -COOL_OFF_SLICES;  // 0 이 되어야 감지한다 (시작 직후 1초도 쉰다)
static uint8_t s_last_prob;
static uint8_t s_cutoff = 247;           // 0.97 * 255
static wake_cb_t s_cb;
static void *s_cb_arg;
static wake_stats_t s_stats;

// microWakeWord 가 학습 때 쓰는 값 (창 30 ms, 간격 10 ms, 40 채널, 125~7500 Hz, 잡음 줄이기, PCAN, 로그)
static esp_err_t frontend_init(void)
{
    s_fe_cfg.window.size_ms = 30;
    s_fe_cfg.window.step_size_ms = 10;
    s_fe_cfg.filterbank.num_channels = FEATURE_SIZE;
    s_fe_cfg.filterbank.lower_band_limit = 125.0f;
    s_fe_cfg.filterbank.upper_band_limit = 7500.0f;
    s_fe_cfg.noise_reduction.smoothing_bits = 10;
    s_fe_cfg.noise_reduction.even_smoothing = 0.025f;
    s_fe_cfg.noise_reduction.odd_smoothing = 0.06f;
    s_fe_cfg.noise_reduction.min_signal_remaining = 0.05f;
    s_fe_cfg.pcan_gain_control.enable_pcan = 1;
    s_fe_cfg.pcan_gain_control.strength = 0.95f;
    s_fe_cfg.pcan_gain_control.offset = 80.0f;
    s_fe_cfg.pcan_gain_control.gain_bits = 21;
    s_fe_cfg.log_scale.enable_log = 1;
    s_fe_cfg.log_scale.scale_shift = 6;
    return FrontendPopulateState(&s_fe_cfg, &s_fe, SAMPLE_RATE) ? ESP_OK : ESP_FAIL;
}

esp_err_t wake_engine_init(const uint8_t *model, size_t model_len, float cutoff, wake_cb_t cb, void *arg)
{
    esp_err_t err = frontend_init();
    if (err != ESP_OK) {
        return err;
    }
    err = tflm_runner_init(model, model_len, TENSOR_ARENA_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    s_stride = tflm_runner_stride();
    s_cb = cb;
    s_cb_arg = arg;
    wake_engine_set_cutoff(cutoff);
    return ESP_OK;
}

void wake_engine_set_cutoff(float cutoff)
{
    if (cutoff < 0.01f) {
        cutoff = 0.01f;
    }
    if (cutoff > 0.99f) {
        cutoff = 0.99f;
    }
    s_cutoff = (uint8_t)(cutoff * 255.0f);
}

float wake_engine_get_cutoff(void)
{
    return s_cutoff / 255.0f;
}

int wake_engine_stride(void)
{
    return s_stride;
}

size_t wake_engine_arena_used(void)
{
    return tflm_runner_arena_used();
}

void wake_engine_take_stats(wake_stats_t *out)
{
    *out = s_stats;
    memset(&s_stats, 0, sizeof(s_stats));
}

// 특징값 한 줄(40개)을 모델 입력에 넣는다. stride 줄이 모이면 한 번 추론한다.
static void feed_features(const uint16_t *values)
{
    int8_t *in = tflm_runner_input() + FEATURE_SIZE * s_step;
    for (int i = 0; i < FEATURE_SIZE; i++) {
        // microfrontend 의 0 ~ 약 666 범위를 int8 로 옮긴다 (TensorFlow Lite 오디오 앞단과 같은 식)
        int32_t v = ((int32_t)values[i] * 256 + 333) / 666 - 128;
        in[i] = (int8_t)(v < -128 ? -128 : v > 127 ? 127 : v);
    }
    s_stats.frames++;
    // 쉬는 시간은 특징값 한 줄(10 ms)마다 센다. 확률이 기준 아래일 때만 줄어든다.
    if (s_last_prob < s_cutoff && s_ignore < 0) {
        s_ignore++;
    }
    if (++s_step < s_stride) {
        return;
    }
    s_step = 0;

    uint8_t p = 0;
    int64_t t0 = esp_timer_get_time();
    if (tflm_runner_invoke(&p) != ESP_OK) {
        return;
    }
    uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);
    s_stats.invokes++;
    s_stats.invoke_us_total += dt;
    if (dt > s_stats.invoke_us_max) {
        s_stats.invoke_us_max = dt;
    }
    if (p > s_stats.prob_max) {
        s_stats.prob_max = p;
    }
    s_last_prob = p;
    s_recent[s_recent_idx] = p;
    s_recent_idx = (s_recent_idx + 1) % SLIDING_WINDOW;

    uint32_t sum = 0;
    uint8_t mx = 0;
    for (int i = 0; i < SLIDING_WINDOW; i++) {
        sum += s_recent[i];
        if (s_recent[i] > mx) {
            mx = s_recent[i];
        }
    }
    uint8_t avg = (uint8_t)(sum / SLIDING_WINDOW);
    if (avg > s_stats.avg_max) {
        s_stats.avg_max = avg;
    }
    if (s_ignore < 0) {
        return;
    }
    if (sum > (uint32_t)s_cutoff * SLIDING_WINDOW) {
        memset(s_recent, 0, sizeof(s_recent));
        s_ignore = -COOL_OFF_SLICES;
        if (s_cb) {
            s_cb(sum / (255.0f * SLIDING_WINDOW), mx / 255.0f, s_cb_arg);
        }
    }
}

void wake_engine_feed(const int16_t *samples, size_t count)
{
    while (count > 0) {
        size_t used = 0;
        int64_t t0 = esp_timer_get_time();
        struct FrontendOutput o = FrontendProcessSamples(&s_fe, samples, count, &used);
        s_stats.frontend_us_total += (uint32_t)(esp_timer_get_time() - t0);
        samples += used;
        count -= used;
        if (o.size == FEATURE_SIZE) {
            feed_features(o.values);
        }
        if (used == 0) {
            break;
        }
    }
}
