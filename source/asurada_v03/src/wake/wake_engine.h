// 호출어 엔진 (C). 16 kHz 소리 샘플을 넣으면 호출어를 들었을 때 알려 준다.
//   소리 -> 특징값 40개 (10 ms 마다, TensorFlow microfrontend) -> int8 -> 스트리밍 모델 -> 확률
//   최근 5번 추론의 평균 확률이 기준을 넘으면 감지. 감지 뒤 1초는 다시 감지하지 않는다.
// 특징값 설정은 microWakeWord 가 학습 때 쓰는 값과 같아야 한다 (wake_engine.c 의 frontend_init).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t frames;            // 만든 특징값 줄 수 (1초에 100)
    uint32_t invokes;           // 추론 횟수
    uint32_t invoke_us_total;   // 추론에 쓴 시간 합 (마이크로초)
    uint32_t invoke_us_max;     // 가장 오래 걸린 추론
    uint32_t frontend_us_total; // 특징값 계산에 쓴 시간 합
    uint8_t prob_max;           // 가장 높았던 확률 (0~255)
    uint8_t avg_max;            // 최근 5번 평균이 가장 높았던 값 (0~255)
} wake_stats_t;

// 감지했을 때 불린다 (wake_engine_feed 를 부른 태스크에서). avg, max 는 0~1
typedef void (*wake_cb_t)(float avg, float max, void *arg);

esp_err_t wake_engine_init(const uint8_t *model, size_t model_len, float cutoff, wake_cb_t cb, void *arg);

// 16 kHz mono 샘플을 넣는다. 길이는 아무래도 된다.
void wake_engine_feed(const int16_t *samples, size_t count);

void wake_engine_set_cutoff(float cutoff);   // 0~1
float wake_engine_get_cutoff(void);

// 지난번 부른 뒤로 모인 통계를 꺼내고 지운다
void wake_engine_take_stats(wake_stats_t *out);

int wake_engine_stride(void);
size_t wake_engine_arena_used(void);

#ifdef __cplusplus
}
#endif
