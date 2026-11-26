# asurada_v03: custom wake word version (source only) / 호출어를 바꾼 판 (소스만)

This is the sketch of the next version of the Asurada Head firmware. It replaces Espressif's "Hi ESP" wake word with a wake word engine that runs a model you train yourself ([microWakeWord](https://github.com/OHF-Voice/micro-wake-word) streaming model on TensorFlow Lite Micro). The maker's figure uses it with the phrase "Hi Asurada".

**The trained wake word model is not included, and there is no ready-made firmware file for this version.** The model was trained with datasets that carry usage restrictions (see the note in the microWakeWord training notebook), so the maker publishes only the source. To use this version you add your own model and build it yourself. The firmware on the install page is still v02 ("Hi ESP").

아스라다 헤드 펌웨어의 다음 판 스케치입니다. Espressif의 "Hi ESP" 호출어 대신, 직접 학습한 모델(microWakeWord 스트리밍 모델, TensorFlow Lite Micro로 실행)을 돌리는 호출어 엔진을 넣었습니다. 제작자의 피규어는 "Hi Asurada"로 씁니다.

**학습한 호출어 모델은 들어 있지 않고, 이 판의 설치용 펌웨어 파일도 없습니다.** 모델을 학습할 때 쓴 자료의 사용 조건 때문에 모델은 공개하지 않고 소스만 올립니다. 이 판을 쓰려면 자기 모델을 넣어 직접 빌드해야 합니다. 설치 페이지의 펌웨어는 그대로 v02("Hi ESP")입니다.

## What changed from v02 / v02와 다른 점

| | v02 | v03 |
|---|---|---|
| Wake word / 호출어 | "Hi ESP" (Espressif WakeNet) | Your own model / 자기 모델 (`src/wake/`) |
| Commands / 명령어 | "One" to "Five" (Espressif ESP-SR) | Same / 같음 |
| New files / 새 파일 | - | `wake.ino`, `src/wake/` |
| New serial command / 새 시리얼 명령 | - | `W90` = detection threshold 0.90 / 판정 기준 0.90 |

Screens, touch, sounds, LEDs and the servo are the same as v02. / 화면, 터치, 소리, LED, 서보는 v02와 같습니다.

How the sound flows / 소리 흐름: microphone → `wake_fill()` in `wake.ino` → (1) the wake word engine, (2) ESP-SR for the commands. When the engine hears the wake word, the sketch switches ESP-SR to command mode and shows LISTENING, exactly as v02 does after "Hi ESP".

Note on "Three" and "Four": `SR_SWAP_34` in `config.h` swaps what these two commands do. With the value in this source (`1`), on the maker's board "Three" starts background music A and "Four" runs the full sequence. Set it to `0` for the opposite.
"Three"와 "Four": `config.h`의 `SR_SWAP_34`가 두 명령의 동작을 맞바꿉니다. 이 소스의 값(`1`)으로는 제작자의 보드에서 "Three"가 배경음악 A, "Four"가 전체 시퀀스입니다. 반대로 하려면 `0`으로 바꿉니다.

## Adding a model / 모델 넣기

1. Train a wake word with [microWakeWord](https://github.com/OHF-Voice/micro-wake-word) (see its `basic_training_notebook.ipynb`), or use a microWakeWord model you have the right to use. Read the notebook's note on the licences of the training data before you share a model.
2. In this folder run:
   ```
   python extras/make_wake_model_header.py your_model.tflite --name "Your Phrase" --cutoff 0.97
   ```
   It overwrites `src/wake/wake_model.h` (the file in this repository only contains an `#error` line).
3. Build and upload with the Arduino IDE, with the same board settings and libraries as v02 (see the README at the top of this repository). TensorFlow Lite Micro comes with the esp32 board package 3.3.10; no extra library is needed.

What the engine expects / 엔진이 기대하는 모델: a microWakeWord streaming model with int8 input of 40 audio features per 10 ms step and a uint8 output (probability). The audio features are computed with the same settings microWakeWord uses for training (`frontend_init` in `src/wake/wake_engine.c`). The wake word is detected when the average of the last 5 inferences is above the threshold; after a detection the engine waits 1 second.

1. [microWakeWord](https://github.com/OHF-Voice/micro-wake-word)로 호출어를 학습하거나(`basic_training_notebook.ipynb`), 쓸 권리가 있는 microWakeWord 모델을 준비합니다. 모델을 남에게 주기 전에 그 노트북에 적힌 학습 자료의 라이선스 안내를 읽어 보세요.
2. 이 폴더에서 위 명령을 실행합니다. `src/wake/wake_model.h`를 다시 만듭니다 (저장소에 있는 파일은 `#error` 한 줄뿐입니다).
3. v02와 같은 보드 설정·라이브러리로 Arduino IDE에서 빌드해 올립니다 (저장소 맨 위 README 참고). TensorFlow Lite Micro는 esp32 보드 패키지 3.3.10에 들어 있어 따로 넣을 라이브러리가 없습니다.

## What was checked / 확인한 것

- The sketch with the maker's model was built (Arduino core 3.3.10) and used on the maker's figure on 2026-11-26: wake word, the commands "One" to "Five", screens, sounds and servo worked.
- **This published copy has not been built**: it is the same sketch with the model file replaced by a placeholder, so it does not compile until you add a model. `extras/make_wake_model_header.py` was checked by regenerating the maker's header from the model file (same model bytes).
- Other voices, long-run false triggers and other models have not been tested systematically. Phrases that differ from the wake word only at the end can still trigger it.

- 제작자의 모델을 넣은 스케치는 2026-11-26에 빌드해(Arduino 코어 3.3.10) 제작자의 피규어에서 호출어, 명령어 "One" ~ "Five", 화면, 소리, 서보가 동작하는 것을 확인했습니다.
- **여기 올린 사본은 빌드해 보지 않았습니다.** 모델 파일만 안내용 빈 파일로 바꾼 같은 스케치라, 모델을 넣기 전에는 컴파일되지 않습니다. `extras/make_wake_model_header.py`는 제작자의 모델 파일로 헤더를 다시 만들어 모델 바이트가 같은 것을 확인했습니다.
- 다른 사람 목소리, 긴 시간의 잘못된 감지, 다른 모델은 체계적으로 시험하지 않았습니다. 호출어와 끝만 다른 말에는 반응할 수 있습니다.

## Licences of the files in `src/wake/` / `src/wake/` 파일의 라이선스

- Audio feature code (`frontend*`, `filterbank*`, `fft*`, `window*`, `noise_reduction*`, `pcan_gain_control*`, `log_*`, `bits.h`): TensorFlow microfrontend, with modifications by Kevin Ahrendt. Apache License 2.0, text in `src/wake/LICENSE`.
- `kiss_fft*`, `_kiss_fft_guts.h`: KISS FFT by Mark Borgerding, BSD 3-Clause (text in the file headers).
- `wake_engine.c/.h`, `tflm_runner.cpp/.h`, `wake.ino`: written for this project.
