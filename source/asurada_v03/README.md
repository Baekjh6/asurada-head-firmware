# asurada_v03: sketch source / 스케치 소스

This is the sketch of the firmware on the install page. Compared with v02 it replaces Espressif's "Hi ESP" wake word with a wake word engine that runs a self-trained model ([microWakeWord](https://github.com/OHF-Voice/micro-wake-word) streaming model on TensorFlow Lite Micro). The model in this source listens for "Hi Asurada".

설치 페이지에 있는 펌웨어의 스케치입니다. v02와 달리 Espressif의 "Hi ESP" 호출어 대신, 직접 학습한 모델(microWakeWord 스트리밍 모델, TensorFlow Lite Micro로 실행)을 돌리는 호출어 엔진을 넣었습니다. 이 소스에 들어 있는 모델은 "Hi Asurada"를 듣습니다.

## The wake word model: non-commercial personal use only / 호출어 모델: 비상업 개인용

`src/wake/wake_model.h` holds the trained model (hi_asurada_v4, 62,304 bytes). It was trained by the maker with microWakeWord, using synthetic speech, the maker's own recordings and the data sets the microWakeWord training notebook downloads. That notebook says the data has mixed licences and usage restrictions, and that models trained with it "should be considered as appropriate for non-commercial personal use only". **The model is offered on those terms: for non-commercial personal use only.** This applies to the model and to firmware files that contain it, not to the rest of the source. See `THIRD-PARTY-NOTICES.txt` at the top of the repository.

`src/wake/wake_model.h`에는 학습한 모델(hi_asurada_v4, 62,304바이트)이 들어 있습니다. 제작자가 microWakeWord로 학습했고, 합성 음성, 제작자의 녹음, microWakeWord 학습 노트북이 내려받는 자료를 썼습니다. 그 노트북은 자료의 라이선스와 사용 조건이 섞여 있어 그것으로 학습한 모델은 "비상업 개인용으로 봐야 한다"고 적고 있습니다. **이 모델은 그 조건으로 제공합니다: 비상업 개인용으로만 쓰세요.** 이 조건은 모델과 모델이 들어간 펌웨어 파일에 해당하고, 나머지 소스에는 해당하지 않습니다. 저장소 맨 위의 `THIRD-PARTY-NOTICES.txt`를 보세요.

The model was trained mostly on one voice (the maker's) plus synthetic voices. It may respond less well to other voices, and phrases that differ from the wake word only at the end ("Hi Asura") can still trigger it.
이 모델은 주로 한 사람(제작자)의 목소리와 합성 음성으로 학습했습니다. 다른 목소리에는 덜 반응할 수 있고, 호출어와 끝만 다른 말("하이 아스라")에도 반응할 수 있습니다.

## What changed from v02 / v02와 다른 점

| | v02 | v03 |
|---|---|---|
| Wake word / 호출어 | "Hi ESP" (Espressif WakeNet) | "Hi Asurada" (own model / 직접 학습한 모델, `src/wake/`) |
| Commands / 명령어 | "One" to "Five" (Espressif ESP-SR) | Same engine / 같은 엔진 |
| "Three" / "Four" | Full sequence / music A · 전체 시퀀스 / 배경음악 A | Music A / full sequence · 배경음악 A / 전체 시퀀스 |
| New files / 새 파일 | - | `wake.ino`, `src/wake/` |
| New serial command / 새 시리얼 명령 | - | `W90` = detection threshold 0.90 / 판정 기준 0.90 |

Screens, touch, sounds, LEDs and the servo are the same as v02. / 화면, 터치, 소리, LED, 서보는 v02와 같습니다.

How the sound flows / 소리 흐름: microphone → `wake_fill()` in `wake.ino` → (1) the wake word engine, (2) ESP-SR for the commands. When the engine hears the wake word, the sketch switches ESP-SR to command mode and shows LISTENING.

"Three" and "Four": `SR_SWAP_34` in `config.h` swaps what these two commands do. With the value in this source (`1`), "Three" starts background music A and "Four" runs the full sequence. Set it to `0` for the opposite.
"Three"와 "Four": `config.h`의 `SR_SWAP_34`가 두 명령의 동작을 맞바꿉니다. 이 소스의 값(`1`)으로는 "Three"가 배경음악 A, "Four"가 전체 시퀀스입니다. 반대로 하려면 `0`으로 바꿉니다.

## Using your own wake word / 자기 호출어 쓰기

1. Train a wake word with [microWakeWord](https://github.com/OHF-Voice/micro-wake-word) (see its `basic_training_notebook.ipynb`), or use a microWakeWord model you have the right to use.
2. In this folder run:
   ```
   python extras/make_wake_model_header.py your_model.tflite --name "Your Phrase" --cutoff 0.97
   ```
   It overwrites `src/wake/wake_model.h`.
3. Build and upload with the Arduino IDE, with the board settings and libraries listed in the README at the top of this repository. TensorFlow Lite Micro comes with the esp32 board package 3.3.10; no extra library is needed.

What the engine expects / 엔진이 기대하는 모델: a microWakeWord streaming model with int8 input of 40 audio features per 10 ms step and a uint8 output (probability). The audio features are computed with the same settings microWakeWord uses for training (`frontend_init` in `src/wake/wake_engine.c`). The wake word is detected when the average of the last 5 inferences is above the threshold; after a detection the engine waits 1 second.

1. [microWakeWord](https://github.com/OHF-Voice/micro-wake-word)로 호출어를 학습하거나(`basic_training_notebook.ipynb`), 쓸 권리가 있는 microWakeWord 모델을 준비합니다.
2. 이 폴더에서 위 명령을 실행합니다. `src/wake/wake_model.h`를 다시 만듭니다.
3. 저장소 맨 위 README에 적힌 보드 설정·라이브러리로 Arduino IDE에서 빌드해 올립니다. TensorFlow Lite Micro는 esp32 보드 패키지 3.3.10에 들어 있어 따로 넣을 라이브러리가 없습니다.

## What was checked / 확인한 것

- This sketch was built (Arduino core 3.3.10) and used on the maker's figure on 2026-11-26: wake word, the commands "One" to "Five", screens, sounds and servo worked.
- The firmware files in `firmware/` are a separate build of the same source.
- `extras/make_wake_model_header.py` was checked by regenerating this header from the model file (same model bytes). Other models have not been tried in this sketch.
- Other voices and long-run false triggers have not been tested systematically.

- 이 스케치는 2026-11-26에 빌드해(Arduino 코어 3.3.10) 제작자의 피규어에서 호출어, 명령어 "One" ~ "Five", 화면, 소리, 서보가 동작하는 것을 확인했습니다.
- `firmware/`의 펌웨어 파일은 같은 소스를 따로 빌드한 것입니다.
- `extras/make_wake_model_header.py`는 모델 파일로 이 헤더를 다시 만들어 모델 바이트가 같은 것을 확인했습니다. 다른 모델은 이 스케치에 넣어 보지 않았습니다.
- 다른 사람 목소리와 긴 시간의 잘못된 감지는 체계적으로 시험하지 않았습니다.

## Licences of the files in `src/wake/` / `src/wake/` 파일의 라이선스

- `wake_model.h`: the trained model, non-commercial personal use only (above). / 학습한 모델, 비상업 개인용 (위 참고).
- Audio feature code (`frontend*`, `filterbank*`, `fft*`, `window*`, `noise_reduction*`, `pcan_gain_control*`, `log_*`, `bits.h`): TensorFlow microfrontend, with modifications by Kevin Ahrendt. Apache License 2.0, text in `src/wake/LICENSE`.
- `kiss_fft*`, `_kiss_fft_guts.h`: KISS FFT by Mark Borgerding, BSD 3-Clause (text in the file headers).
- `wake_engine.c/.h`, `tflm_runner.cpp/.h`, `wake.ino`: written for this project.
