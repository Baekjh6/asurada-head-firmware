// TensorFlow Lite Micro 를 C 에서 쓰기 위한 얇은 껍데기.
// TFLite Micro 는 C++ 로만 제공되므로 이 파일 한 쌍(tflm_runner.h / .cc)만 C++ 이고, 나머지는 모두 C 다.
// 대상: microWakeWord 스트리밍 모델 (입력 [1, stride, 40] int8, 출력 [1, 1] uint8)
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// model: .tflite 내용 (살아 있는 동안 지워지면 안 된다). arena_size: 작업 메모리 크기(바이트)
esp_err_t tflm_runner_init(const uint8_t *model, size_t model_len, size_t arena_size);

// 한 번 추론에 넣는 특징값 줄 수 (모델 입력의 가운데 차원)
int tflm_runner_stride(void);

// 모델 입력 버퍼 (stride x 40 개의 int8). 여기에 채우고 tflm_runner_invoke 를 부른다.
int8_t *tflm_runner_input(void);

// 추론 한 번. 확률(0~255)을 prob 에 넣는다.
esp_err_t tflm_runner_invoke(uint8_t *prob);

// 실제로 쓴 작업 메모리(바이트)
size_t tflm_runner_arena_used(void);

#ifdef __cplusplus
}
#endif
