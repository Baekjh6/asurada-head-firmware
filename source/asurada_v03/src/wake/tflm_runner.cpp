// TensorFlow Lite Micro 껍데기 (이 프로젝트에서 C++ 인 파일은 이것 하나다). 설명은 tflm_runner.h
#include "tflm_runner.h"

#include <new>

#include "esp_heap_caps.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
// schema_generated.h 는 micro_interpreter.h 가 불러온다 (TFLite Micro 판마다 위치가 달라 직접 부르지 않는다)

#define FEATURE_SIZE 40
#define VAR_ARENA_SIZE 1024    // 스트리밍 모델이 상태를 담아 두는 변수용
#define VAR_COUNT 20

static tflite::MicroMutableOpResolver<20> s_resolver;
static tflite::MicroInterpreter *s_interp = nullptr;
alignas(16) static uint8_t s_interp_mem[sizeof(tflite::MicroInterpreter)];
static int s_stride = 0;

esp_err_t tflm_runner_init(const uint8_t *model_data, size_t model_len, size_t arena_size)
{
    (void)model_len;
    if (s_interp != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    // microWakeWord 스트리밍 모델이 쓰는 연산 20가지
    s_resolver.AddCallOnce();
    s_resolver.AddVarHandle();
    s_resolver.AddReshape();
    s_resolver.AddReadVariable();
    s_resolver.AddStridedSlice();
    s_resolver.AddConcatenation();
    s_resolver.AddAssignVariable();
    s_resolver.AddConv2D();
    s_resolver.AddMul();
    s_resolver.AddAdd();
    s_resolver.AddMean();
    s_resolver.AddFullyConnected();
    s_resolver.AddLogistic();
    s_resolver.AddQuantize();
    s_resolver.AddDepthwiseConv2D();
    s_resolver.AddAveragePool2D();
    s_resolver.AddMaxPool2D();
    s_resolver.AddPad();
    s_resolver.AddPack();
    s_resolver.AddSplitV();

    uint8_t *var_arena = (uint8_t *)heap_caps_aligned_alloc(16, VAR_ARENA_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint8_t *arena = (uint8_t *)heap_caps_aligned_alloc(16, arena_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (var_arena == nullptr || arena == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    tflite::MicroAllocator *ma = tflite::MicroAllocator::Create(var_arena, VAR_ARENA_SIZE);
    tflite::MicroResourceVariables *mrv = tflite::MicroResourceVariables::Create(ma, VAR_COUNT);
    if (ma == nullptr || mrv == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    const tflite::Model *model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    s_interp = new (s_interp_mem) tflite::MicroInterpreter(model, s_resolver, arena, arena_size, mrv);
    if (s_interp->AllocateTensors() != kTfLiteOk) {
        s_interp = nullptr;
        return ESP_FAIL;
    }
    TfLiteTensor *in = s_interp->input(0);
    TfLiteTensor *out = s_interp->output(0);
    if (in->type != kTfLiteInt8 || in->dims->size != 3 || in->dims->data[0] != 1 || in->dims->data[2] != FEATURE_SIZE ||
        out->type != kTfLiteUInt8) {
        s_interp = nullptr;
        return ESP_ERR_INVALID_ARG;
    }
    s_stride = in->dims->data[1];
    return ESP_OK;
}

int tflm_runner_stride(void)
{
    return s_stride;
}

int8_t *tflm_runner_input(void)
{
    return s_interp ? tflite::GetTensorData<int8_t>(s_interp->input(0)) : nullptr;
}

esp_err_t tflm_runner_invoke(uint8_t *prob)
{
    if (s_interp == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_interp->Invoke() != kTfLiteOk) {
        return ESP_FAIL;
    }
    *prob = s_interp->output(0)->data.uint8[0];
    return ESP_OK;
}

size_t tflm_runner_arena_used(void)
{
    return s_interp ? s_interp->arena_used_bytes() : 0;
}
