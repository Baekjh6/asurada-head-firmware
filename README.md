# Asurada Head firmware / 아스라다 헤드 펌웨어

**Install page / 설치 페이지: https://baekjh6.github.io/asurada-head-firmware/**

Firmware for the **Waveshare ESP32-S3-Touch-AMOLED-1.75** round display board used in the Asurada Head desk figure (standby eyes, speed counter, two booster countdown screens, five LEDs, a 360-degree servo, English voice commands after "Hi ESP"). **No sound files are included.**
Asurada Head 탁상 피규어에 쓰는 Waveshare ESP32-S3-Touch-AMOLED-1.75 원형 디스플레이 보드용 펌웨어입니다 (대기화면의 눈, 속도 숫자, 부스터 카운트다운 화면 2개, LED 5개, 360도 서보, "Hi ESP" 뒤 영어 음성 명령). **소리 파일은 포함되어 있지 않습니다.**

This is an unofficial fan-made project based on the anime Cyber Formula. It is not affiliated with or endorsed by the rights holders.
애니메이션 사이버 포뮬러를 소재로 한 비공식 팬 제작 프로젝트이며, 권리자와 관련이 없고 승인을 받은 것이 아닙니다.

## 1. Easiest: install from the browser / 가장 쉬운 방법: 브라우저에서 설치

Open `index.html` from its **https://** address in **Chrome or Edge on a PC**, connect the figure with a USB data cable, click **Install firmware**.
**https:// 주소**로 올린 `index.html`을 **PC의 Chrome 또는 Edge**에서 열고, USB 데이터 케이블로 피규어를 연결한 뒤 **펌웨어 설치**를 누릅니다.

> Use the USB socket on the stand if its four wires (5V, GND, D+, D-) are connected to the display board as in the assembly guide. Otherwise use the display board's own USB-C port (the screen bezel is held by magnets and comes off).
> 조립 설명서대로 선 4가닥(5V, GND, D+, D-)을 화면 보드에 연결했다면 받침대의 USB 소켓을 씁니다. 그렇지 않으면 화면 보드 자체의 USB-C 포트를 씁니다 (화면 테두리는 자석으로 붙어 있어 떼어 낼 수 있습니다).

## 2. With esptool / esptool 로 설치

One command writes everything (Python and `pip install esptool` needed). Replace `COM5` with your port (`/dev/ttyACM0` on Linux).
한 줄로 전부 씁니다 (Python 과 `pip install esptool` 필요). `COM5` 는 자기 포트로 바꾸세요.

```powershell
python -m esptool --chip esp32s3 --port COM5 write_flash 0x0 asurada-head-full-0x0.bin
```

`asurada-head-full-0x0.bin` (about 16 MB) is in `firmware/`. It contains the five files below, already placed at their offsets (flash mode DIO, 80 MHz, 16 MB).
`firmware/` 의 `asurada-head-full-0x0.bin` (약 16 MB)은 아래 5개 파일을 위치에 맞게 합친 것입니다 (플래시 DIO, 80 MHz, 16 MB).

## 3. With the Espressif Flash Download Tool (Windows) / Flash Download Tool(윈도우)

Choose chip **ESP32-S3**, then either add the single file `asurada-head-full-0x0.bin` at address `0x0`, or add the five files below at their addresses, and set SPI mode **DIO**, SPI speed **80 MHz**, flash size **16 MB** (128 Mbit).
칩은 **ESP32-S3**, 파일은 합친 `asurada-head-full-0x0.bin`을 `0x0`에 넣거나 아래 5개를 각 주소에 넣고, SPI 모드 **DIO**, 속도 **80 MHz**, 크기 **16 MB**(128 Mbit)로 설정합니다.

## Files and flash addresses / 파일과 굽는 주소

| Address | File | What it is / 내용 |
|---|---|---|
| `0x0` | `firmware/bootloader.bin` | Bootloader / 부트로더 |
| `0x8000` | `firmware/partition-table.bin` | Partition table (Arduino "ESP SR 16M") / 파티션 표 |
| `0xe000` | `firmware/boot_app0.bin` | Selects the first app slot / 첫 번째 앱 자리를 고르는 파일 |
| `0x10000` | `firmware/asurada-head-app.bin` | The figure's program / 피규어 프로그램 |
| `0xc10000` | `firmware/voice-models.bin` | Voice models ("Hi ESP" wake word and English commands) / 음성 모델 |

`firmware/SHA256SUMS.txt` lists the checksums. Check one on Windows with `Get-FileHash firmware\asurada-head-app.bin`.
체크섬은 `firmware/SHA256SUMS.txt` 에 있습니다. 윈도우에서는 `Get-FileHash firmware\asurada-head-app.bin` 으로 확인합니다.

## After the install / 설치 뒤

The head restarts and shows four green eyes. It needs no Wi-Fi. Without a microSD card it runs silently.
머리가 다시 시작하고 초록 눈 4개가 나옵니다. Wi-Fi는 필요 없습니다. microSD 카드가 없으면 소리 없이 동작합니다.

| You do / 하는 것 | The figure does / 동작 |
|---|---|
| Tap on standby / 대기화면에서 터치 | Speed counter 0 to 200 / 속도 숫자 0 → 200 |
| Tap on the speed counter / 속도 숫자 화면에서 터치 | Booster stage 1 (9 s) / 1단 부스터 (9초) |
| Tap during stage 1 / 1단 중에 터치 | Stage 1, then stage 2 (5 s) / 1단에 이어 2단 (5초) |
| Hold 3 s on standby / 대기화면에서 3초 길게 누르기 | Speed counter, stage 1, stage 2 / 속도 숫자, 1단, 2단 |
| Swipe up or down / 위·아래로 밀기 | Volume / 볼륨 |
| Swipe right or left on standby / 대기화면에서 오른쪽·왼쪽으로 밀기 | Background music A or B; tap to stop / 배경음악 A·B, 터치하면 정지 |
| "Hi ESP", then "One" / "Two" / "Three" | Stage 1 / stage 1 and 2 / full sequence |
| "Hi ESP", then "Four" / "Five" | Background music A / B |

## Sounds on the microSD card / microSD 카드의 소리

No sound files are included. Format the card as FAT32, make the folders `/1` to `/8` and put one WAV file into each. The first `.wav` in a folder is played; the file name is free. Format: 16-bit PCM WAV, mono or stereo, 8 kHz or higher (converted to 16 kHz mono while playing). MP3 does not play. Use sounds you made yourself or have the right to use.
소리 파일은 포함되어 있지 않습니다. 카드를 FAT32로 포맷하고 `/1`~`/8` 폴더를 만든 뒤 폴더마다 WAV 파일 하나를 넣습니다. 폴더의 첫 번째 `.wav`를 재생하고 파일 이름은 자유입니다. 형식은 16비트 PCM WAV, 모노 또는 스테레오, 8 kHz 이상입니다 (재생하면서 16 kHz 모노로 바꿉니다). MP3는 재생되지 않습니다. 직접 만들었거나 쓸 권리가 있는 소리를 쓰세요.

| Folder | Played when / 재생되는 때 | Screen length / 화면 길이 |
|---|---|---|
| `/1` | The figure starts / 켜질 때 | - |
| `/2` | A tap opens the speed counter / 터치로 속도 숫자 화면이 열릴 때 | - |
| `/3` | The board's PWR button / 보드의 PWR 버튼 | - |
| `/4` | Booster stage 1 (tap on the speed counter, or "One") / 1단 부스터 | 9 s |
| `/5` | Stage 1 and 2 (tap during stage 1, or "Two") / 1단 + 2단 | 14 s |
| `/6` | Full sequence (hold 3 s, or "Three") / 전체 시퀀스 | 17 s |
| `/7` | Background music A (swipe right, or "Four"), repeats / 배경음악 A, 반복 | - |
| `/8` | Background music B (swipe left, or "Five"), repeats / 배경음악 B, 반복 | - |

## Wiring the firmware expects / 펌웨어가 쓰는 배선

| Part / 부품 | Pin / 핀 |
|---|---|
| Servo signal (SG90, 360-degree) / 서보 신호 | GPIO18 |
| LED data, 5 addressable LEDs (NeoPixel, GRB, 800 kHz) / LED 신호 | GPIO16 |
| LED order / LED 순서 | 0 = under the face, 1 and 2 = right wing, 3 and 4 = left wing / 0 = 얼굴 아래, 1·2 = 오른쪽 날개, 3·4 = 왼쪽 날개 |

Everything else (display, touch, audio codec, microphones, power chip, motion sensor, microSD) is on the display board.
나머지(화면, 터치, 오디오 코덱, 마이크, 전원 칩, 움직임 센서, microSD)는 화면 보드에 있습니다.

## Serial commands (115200 baud) / 시리얼 명령

| Command | Meaning / 뜻 |
|---|---|
| `D35` | Head-turn speed 35 degrees per second / 고개 돌리는 속도 35도/초 |
| `A30` | Head-turn angle 30 degrees / 돌아갈 각도 30도 |
| `M34` | Pulse offset at which the servo starts to move / 서보가 움직이기 시작하는 세기 |
| `T` | Turn once now / 지금 한 번 동작 |
| `R` | Back to the values in `config.h` / `config.h` 의 값으로 되돌림 |
| `+` `-` | Shift the screen against the sound by 20 ms / 소리와 화면을 20 ms씩 맞춤 |
| `1` `2` `3` | Same as saying "One", "Two", "Three" / "One", "Two", "Three"와 같음 |

Changed values are saved on the board. / 바꾼 값은 보드에 저장됩니다.

## Building from source / 소스에서 빌드

> **v03 (source only):** `source/asurada_v03` is the next version, with a wake word engine for a model you train yourself ("Hi Asurada" on the maker's figure). The trained model is not included and there is no firmware file for it; see `source/asurada_v03/README.md`. Everything else in this README describes v02, the firmware in `firmware/`.
> **v03 (소스만):** `source/asurada_v03` 은 직접 학습한 모델을 돌리는 호출어 엔진을 넣은 다음 판입니다 (제작자의 피규어에서는 "Hi Asurada"). 학습한 모델과 설치용 펌웨어 파일은 들어 있지 않습니다. `source/asurada_v03/README.md` 를 보세요. 이 README의 나머지는 `firmware/` 에 있는 v02 기준입니다.

The sketch is in `source/asurada_v02`. / 스케치는 `source/asurada_v02` 에 있습니다.

- Arduino IDE with the **esp32** board package 3.3.10 / Arduino IDE와 **esp32** 보드 패키지 3.3.10
- Tools: Board **ESP32S3 Dev Module**, Flash Size **16MB**, PSRAM **OPI PSRAM**, Partition Scheme **ESP SR 16M (3MB APP/7MB SPIFFS/2.9MB MODEL)**, USB CDC On Boot **Enabled**
- Libraries / 라이브러리: lvgl 8.4.0, GFX Library for Arduino 1.6.4, SensorLib 0.3.1, XPowersLib 0.2.6, Adafruit NeoPixel 1.15.5
- Copy `source/asurada_v02/setup/lv_conf.h` into your `Arduino/libraries/` folder (next to the `lvgl` folder). LVGL looks for it there. / `source/asurada_v02/setup/lv_conf.h` 를 `Arduino/libraries/` 폴더(`lvgl` 폴더 옆)에 복사합니다. LVGL이 그 자리에서 찾습니다.
- Settings you can change are collected in `config.h`. `assets.h` is generated and is not meant to be edited by hand (the generator script is not part of this package). / 바꿀 수 있는 값은 `config.h` 에 모여 있습니다. `assets.h` 는 자동으로 만든 파일이라 손으로 고치지 않습니다 (만드는 스크립트는 이 묶음에 없습니다).
- With the ESP SR partition scheme the Arduino IDE also writes the voice models when it uploads. / ESP SR 파티션을 고르면 Arduino IDE가 올릴 때 음성 모델도 같이 씁니다.

## Notes / 참고

- Licences of the components used: `THIRD-PARTY-NOTICES.txt`. The in-browser installer is [ESP Web Tools](https://github.com/esphome/esp-web-tools) (Apache-2.0, copy in `vendor/`). / 사용한 구성 요소의 라이선스는 `THIRD-PARTY-NOTICES.txt`, 브라우저 설치 도구는 ESP Web Tools(Apache-2.0, `vendor/`에 사본).
- Display board: Waveshare ESP32-S3-Touch-AMOLED-1.75 (not affiliated). / 디스플레이 보드: Waveshare ESP32-S3-Touch-AMOLED-1.75 (제휴 아님).
