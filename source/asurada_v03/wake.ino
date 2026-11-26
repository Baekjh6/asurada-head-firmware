// ==========================================================================
// 호출어 "Hi Asurada" (직접 학습한 모델). v02 의 "Hi ESP"(Espressif WakeNet) 자리를 대신한다.
//
//   엔진     : src/wake/  (wakeword/idf/asurada_wake 의 ESP-IDF 프로젝트와 같은 C 소스.
//              TensorFlow Lite Micro 를 감싸는 tflm_runner.cpp 하나만 C++)
//   모델     : src/wake/wake_model.h  (wakeword/tools/install_model.py --arduino 가 만든다)
//   소리 흐름: 마이크(I2S) -> wake_fill() -+-> 우리 엔진 (호출어)
//                                          +-> ESP-SR 앞단 -> 명령어 인식 (One ~ Five)
//   ESP-SR 의 호출어(WakeNet)는 꺼 둔다(SR_MODE_OFF). 우리 엔진이 호출어를 들으면
//   v02 가 "Hi ESP" 를 들었을 때와 똑같이 명령어 듣기(SR_MODE_COMMAND)로 넘긴다.
//
//   시리얼 명령: W숫자 = 판정 기준(%), 예) W90 -> 0.90   (기본값은 wake_model.h 의 WAKE_MODEL_CUTOFF)
// ==========================================================================
#include "src/wake/wake_engine.h"
#include "src/wake/wake_model.h"

static volatile uint32_t wake_count = 0;
static volatile float wake_last_avg = 0;
static volatile bool wake_note = false;

// 호출어를 들었을 때 (ESP-SR 의 소리 넣기 Task 에서 불린다 -> 화면/SD/I2S 는 건드리지 않는다)
static void on_wake(float avg, float mx, void *arg) {
  (void)mx;
  (void)arg;
  wake_count = wake_count + 1;
  wake_last_avg = avg;
  wake_note = true;                             // 글자는 seq Task 가 찍는다 (wake_report)
  if (isPlayingWav || seq_auto) return;         // 소리가 나거나 시퀀스 중에는 듣지 않음 (v02 와 같음)
  if (g_ui == UI_LISTEN) return;                // 이미 명령어를 듣는 중
  g_ui = UI_LISTEN;
  ESP_SR.setMode(SR_MODE_COMMAND);
}

// ESP-SR 이 마이크 소리를 달라고 부른다. 읽은 소리를 우리 엔진에도 넣는다.
static esp_err_t wake_fill(void *arg, void *out, size_t len, size_t *bytes_read, uint32_t timeout_ms) {
  (void)arg;
  i2s.setTimeout(timeout_ms);
  *bytes_read = i2s.readBytes((char *)out, len);
  esp_err_t err = (esp_err_t)i2s.lastError();
  if (err == ESP_OK && *bytes_read >= 2) wake_engine_feed((const int16_t *)out, *bytes_read / 2);
  return err;
}

static void wake_sr_event(void *arg, sr_event_t event, int command_id, int phrase_id) {
  (void)arg;
  onSrEvent(event, command_id, phrase_id);
}

// 음성인식 시작: 우리 호출어 엔진 + ESP-SR(명령어). v02 의 ESP_SR.begin() 자리에서 부른다.
bool wake_begin(const sr_cmd_t *cmds, size_t count) {
  esp_err_t err = wake_engine_init(wake_model_data, wake_model_len, WAKE_MODEL_CUTOFF, on_wake, NULL);
  if (err != ESP_OK) {
    Serial.printf("[WAKE] 엔진 시작 실패: %s\n", esp_err_to_name(err));
    return false;
  }
  err = sr_start(wake_fill, NULL, SR_CHANNELS_MONO, SR_MODE_WAKEWORD, "M", cmds, count, wake_sr_event, NULL);
  if (err != ESP_OK) {
    Serial.printf("[WAKE] ESP-SR 시작 실패: %s\n", esp_err_to_name(err));
    return false;
  }
  ESP_SR.setMode(SR_MODE_OFF);                  // Espressif 호출어(Hi ESP)는 끈다
  Serial.printf("[WAKE] %s 준비 (모델 %u B, 기준 %.2f, 작업 메모리 %u B)\n", WAKE_MODEL_NAME, (unsigned)wake_model_len,
                wake_engine_get_cutoff(), (unsigned)wake_engine_arena_used());
  return true;
}

void wake_set_cutoff(int percent) {
  wake_engine_set_cutoff(percent / 100.0f);
  Serial.printf("[WAKE] 판정 기준 %.2f\n", wake_engine_get_cutoff());
}

// seq Task 가 돌 때마다 부른다: 호출어를 들었으면 한 줄 찍는다
void wake_report() {
  if (!wake_note) return;
  wake_note = false;
  Serial.printf("[WAKE] %s  avg=%.2f  (count %lu)\n", WAKE_MODEL_NAME, (float)wake_last_avg, (unsigned long)wake_count);
}
