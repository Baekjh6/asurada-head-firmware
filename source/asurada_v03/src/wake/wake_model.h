// The trained wake word model is NOT included in this repository.
// 학습한 호출어 모델은 이 저장소에 들어 있지 않습니다.
//
// To build this sketch, put your own microWakeWord model here:
//   python extras/make_wake_model_header.py your_model.tflite --name "Your Phrase" --cutoff 0.97
// It writes this file (src/wake/wake_model.h) with:
//   WAKE_MODEL_NAME, WAKE_MODEL_CUTOFF, wake_model_len and the model bytes as a C array.
// See README.md in the sketch folder.
//
// 이 스케치를 빌드하려면 자기 microWakeWord 모델을 넣습니다. 위 명령이 이 파일을 다시 만듭니다.
// 스케치 폴더의 README.md 를 보세요.
#pragma once
#error "src/wake/wake_model.h has no wake word model. Run extras/make_wake_model_header.py with your own .tflite model (see README.md)."
