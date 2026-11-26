// ==========================================================================
// 아스라다 헤드 v03 - Waveshare ESP32-S3-Touch-AMOLED-1.75
//
// v03 에서 바뀐 것: 호출어가 "Hi ESP" -> "Hi Asurada" (직접 학습한 모델, wake.ino + src/wake/).
//   호출한 뒤의 동작(명령어 One ~ Five, 화면, 소리, 서보)은 v02 그대로다.
//
// [Tools 설정 - 필수]  Board: ESP32S3 Dev Module / Flash 16MB / PSRAM: OPI PSRAM /
//                      Partition Scheme: ESP SR 16M (3MB APP/...) / USB CDC On Boot: Enabled
// [라이브러리]         lvgl 8.4.0 (setup/lv_conf.h 를 Arduino/libraries/ 폴더에 복사),
//                      GFX Library for Arduino, SensorLib, XPowersLib, Adafruit NeoPixel
// [SD 카드]            /1 ~ /8 폴더에 PCM WAV(16bit). 폴더마다 첫 번째 .wav 를 재생합니다 (파일 이름 자유).
//                      SD 가 없어도 화면은 모두 동작합니다 (배경 그림은 펌웨어에 들어 있음).
//
// v02 에서 바뀐 것 (참고 영상에 맞춤)
//   - 속도계: 큰 숫자 0 -> 200 (3초) + 좌우 막대
//   - 1단: 꽃잎 9장에서 시작, 숫자 하나에 1초씩 한 장이 어두워지며 사라짐 (14 -> 6). 초록/빨강 게이지가 자람
//   - 1단 -> 2단: 2단 화면이 오른쪽에서 왼쪽으로 밀고 들어옴
//   - 2단: 12시에서 시작해 반시계로 지워짐. 흰 점이 1초에 한 바퀴 (5 -> 1)
//   - 대기화면: 입체 구슬, 돌면서 등장, 부드러운 회전
//   - 화면 보호(어둡게), 볼륨/싱크 저장, 서보 자이로 제어
//   조정값은 config.h, 화면 모양은 tools/gen_assets.py -> assets.h
//
// 파일 구성
//   asurada_v03.ino : 공용 변수, setup(), loop()
//   wake.ino        : 호출어 "Hi Asurada" (엔진은 src/wake/, C)
//   audio.ino       : WAV 재생 Task, 음성인식 콜백
//   seq.ino         : 화면 순서(시퀀스) Task
//   hw.ino          : 코덱/마이크 초기화, 서보 + LED Task
//   ui_common.ino   : 그림 풀기, 다각형, 7세그먼트 숫자
//   ui_standby.ino / ui_speed.ino / ui_boost1.ino / ui_boost2.ino : 화면 4개
//   ui_main.ino     : 화면 전환, 터치, 화면 보호, 설정 저장
//   build_opt.h     : 컴파일 옵션 (디스플레이로 한 번에 보내는 양을 늘려 화면 전송을 빠르게)
//
// Task
//   [loop  / core 1 / prio 6] LVGL 그리기 + 터치 + PWR 버튼 (LVGL 은 이 Task 에서만 호출)
//   [audio / core 0 / prio 6] SD 의 WAV 재생 (16kHz 모노로 변환, 마이크와 I2S 공유)
//   [seq   / core 0 / prio 7] 화면 단계 전환 + 소리 요청
//   [servo / core 0 / prio 1] 서보 + NeoPixel
// ==========================================================================

#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "config.h"
#include "TouchDrvCSTXXX.hpp"
#include <SensorQMI8658.hpp>
#include <Wire.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <math.h>
#include "esp_partition.h"
#include "ESP_I2S.h"     // 재생 + 마이크 겸용 I2S (16kHz)
#include "ESP_SR.h"      // 음성인식 (명령어 One ~ Five. 호출어는 wake.ino 의 "Hi Asurada")
#include "XPowersLib.h"
#include "assets.h"

// ---------------- 형(struct) 정의 : 반드시 첫 번째 함수보다 위에 ----------------
struct AudioCmd { char path[48]; };

struct WavInfo {
  uint16_t channels;
  uint32_t rate;
  uint16_t bits;
  uint32_t dataStart;
  uint32_t dataLen;
};

struct Player {                       // audioTask 전용 재생 상태
  File     f;
  WavInfo  w;
  uint32_t remain = 0;
  uint32_t step = 0;                  // 입력 샘플 증가량 (16.16 고정소수점)
  uint32_t pos = 0;
  int16_t  prev = 0;
  int32_t  lp = 0;                    // 다운샘플 전 저역통과 필터 상태
  int32_t  lp_a = 65536;
  bool     active = false;
  bool     loop = false;
};

struct FlowStep {                     // 시퀀스 한 단계
  uint8_t     ph;                     // 화면 (SC_...)
  uint32_t    dur;                    // 길이 (ms)
  const char *snd;                    // 이 단계가 시작될 때 재생할 폴더 (NULL = 없음)
};

struct SegNum {                       // 7세그먼트 숫자 (오른쪽 정렬)
  lv_obj_t      *obj;
  const int16_t *outer;               // [7][6][2] 조각 좌표
  const int16_t *inner;               // 속을 비우는 조각 (NULL = 꽉 찬 획)
  int16_t        dw, dh, adv;         // 한 자리의 폭/높이, 자리 간격
  uint8_t        ndig;
  int            val, prev;
  uint32_t       chg_ms;
  bool           ghost;               // 이전 숫자의 획을 겹쳐 그리는 중
};

// ---------------- 하드웨어 ----------------
XPowersPMU pmu;
bool pmu_online = false;
SensorQMI8658 qmi;
bool imu_online = false;
Preferences prefs;

#define SERVO_PIN 18
#define LED_PIN 16
#define NUM_LEDS 5
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define ASU_SD_CLK 2
#define ASU_SD_CMD 1
#define ASU_SD_D0  3

#define BOARD_PA_PIN    46
#define BOARD_I2S_MCLK  42
#define BOARD_I2S_SCLK   9
#define BOARD_I2S_LRCK  45
#define BOARD_I2S_DOUT   8
#define BOARD_I2S_DIN   10
#define ES8311_ADDR   0x18
#define ES7210_ADDR   0x40
#define I2S_SAMPLE_RATE  16000     // ESP-SR 이 16kHz 고정이라 재생도 16kHz 로 통일
#define TCA9554_ADDR  0x20
#define EXIO_PWR_BIT  4
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

I2SClass i2s;
int current_volume = DEFAULT_VOLUME;      // 0 ~ 21
volatile int target_volume = DEFAULT_VOLUME;

static QueueHandle_t audioQ = NULL;       // 재생 요청
static QueueHandle_t seqQ = NULL;         // 터치/음성 -> seqTask 요청

// 오디오 시계: audioTask 가 I2S 에 써넣은 샘플 수 -> 재생 위치(ms)
volatile uint32_t au_gen    = 0;          // 새 소리가 실제로 나가기 시작할 때마다 +1
volatile uint32_t au_fail   = 0;          // 파일이 없거나 열지 못했을 때마다 +1
volatile uint32_t au_pos_ms = 0;
volatile uint32_t au_stamp  = 0;
static uint32_t   au_samples = 0;

volatile bool isPlayingWav = false;       // 재생 중이면 음성인식 이벤트 무시 (스피커 소리를 마이크가 듣기 때문)

enum { UI_NONE = 0, UI_READY, UI_LISTEN };
volatile int g_ui = UI_READY;

enum { CMD_NUM_1 = 1, CMD_NUM_2, CMD_NUM_3, CMD_NUM_4, CMD_NUM_5, CMD_NUM_6, CMD_NUM_7, CMD_NUM_8 };
static const sr_cmd_t sr_commands[] = {
  {CMD_NUM_1, "One"},      // 1단 부스터
  {CMD_NUM_2, "Two"},      // 1단 + 2단
  {CMD_NUM_3, "Three"},    // 속도계 + 1단 + 2단
  {CMD_NUM_4, "Four"},     // 배경음악 A
  {CMD_NUM_5, "Five"},     // 배경음악 B
  {CMD_NUM_6, "Six"},
  {CMD_NUM_7, "Seven"},
  {CMD_NUM_8, "Eight"},
};

uint32_t screenWidth;
uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

TouchDrvCST92xx touch;
int16_t x[5], y[5];
volatile bool isPressed = false;

// ---------------- 화면 단계 / 요청 ----------------
enum { SC_STANDBY = 0, SC_SPEED, SC_FAN, SC_CIRC, SC_BLACK };
enum { REQ_NONE = 0, REQ_TAP, REQ_LONG, REQ_BGM7, REQ_BGM8, REQ_STOP, REQ_V1, REQ_V2, REQ_V3 };

// Task 간 공유 상태
volatile int      seq_phase = SC_STANDBY;   // seqTask 가 쓰고 loop 가 읽음
volatile uint32_t phase_ms  = 0;            // 현재 단계 시작 시각
volatile bool     seq_auto  = false;        // 시퀀스 진행 중
volatile int      sync_trim = SYNC_TRIM_MS;
volatile int      sr_last_cmd = 0, sr_last_phrase = 0;
volatile int      seq_bgm   = 0;            // 배경음악 재생 중: 0 = 없음, 7 / 8
volatile bool     cfg_dirty = false;        // 볼륨/싱크 값이 바뀜 -> loop 가 저장

// 자이로 각도 측정 (loop Task 가 읽고 쌓음, servoTask 는 켜고 끄기만)
volatile uint8_t  gyro_cmd = 0;             // servoTask -> loop : 1 = 측정 켬, 0 = 끔
volatile uint8_t  gyro_state = 0;           // loop -> servoTask : 0 = 꺼짐, 1 = 영점 재는 중, 2 = 각도 쌓는 중
volatile float    gyro_ang[3] = {0, 0, 0};  // 켠 뒤로 돈 각도 (도)
volatile uint32_t gyro_samples = 0;

// 서보 조정값 (config.h 가 초기값. 시리얼 명령으로 바꾸면 저장되어 그 값이 우선)
volatile int      servo_dps      = SERVO_DPS;       // 목표 속도 (도/초)
volatile int      servo_min_us   = SERVO_MIN_US;
volatile int      servo_turn_deg = (int)SERVO_TURN_DEG;
volatile bool     servo_trigger  = false;   // true = 지금 한 번 움직임 (시리얼 'T')
volatile bool     servo_relearn  = false;   // true = 배운 값(움직이기 시작하는 세기 등)을 처음으로

// ---------------- UI 객체 (loop Task 에서만 접근) ----------------
lv_obj_t *standby_cont, *speed_cont, *boost1_cont, *boost2_cont;
lv_obj_t *listen_lbl;

// 대기화면
lv_obj_t *orb_img[4];
static lv_img_dsc_t orb_dsc;
float    orb_ang = 45.0f, orb_rad = ORB_RADIUS;
float    orb_from_ang = 45.0f, orb_to_ang = 45.0f, orb_from_rad = ORB_RADIUS;
uint32_t orb_anim_ms = 0, orb_anim_len = 0;
bool     orb_anim_spin = false;
uint32_t orb_turn_ms = 0, shift_ms = 0;
int      shift_idx = 0, shift_x = 0, shift_y = 0;

// 속도계
SegNum    sp_num;
lv_obj_t *sp_arc[3];                        // 0 = 왼쪽 하늘색, 1 = 오른쪽 초록, 2 = 오른쪽 빨강
float     sp_bar[3] = {30, 14, 12}, sp_bar_to[3] = {30, 14, 12};
int       sp_bar_shown[3] = {-1, -1, -1};
uint32_t  sp_bar_ms = 0;

// 1단 부스터
lv_obj_t *b1_layer;
static lv_img_dsc_t b1_bg_dsc;
static bool b1_bg_ok = false;
SegNum    b1_num;
uint32_t  b1_start_ms = 0;
int       b1_cells = B1_CELLS;              // 지금 보이는 꽃잎 수
int       b1_fade_idx = -1, b1_fade_k = 0;  // 사라지는 중인 꽃잎과 밝기 (0~255)
int       b1_green = 0, b1_red2 = 0, b1_leaf = 0;

// 2단 부스터
lv_obj_t *b2_layer;
static lv_img_dsc_t b2_bg_dsc;
static bool b2_bg_ok = false;
SegNum    b2_num;
uint32_t  b2_start_ms = 0;
float     b2_edge = 270.0f;                 // 빨간 영역이 270도(12시)에서 시계 방향으로 여기까지 있음
int       b2_lit = 0;                       // 켜진 흰 점 개수
bool      b2_deco = false;                  // 화살표 표시

// 터치
uint32_t press_start_ms = 0;
uint32_t last_touch_ms = 0;
bool is_touching = false;
int16_t start_x = 0;
int16_t start_y = 0;
bool touch_consumed = false;
bool is_swipe = false;
bool long_press_handled = false;

// 화면 보호
uint32_t last_act_ms = 0;
int      screen_level = SCREEN_BRIGHT;

static const uint8_t DIGIT_MASK[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};

// setup() 안에서 ESP-SR 모델/명령어 트리를 만들 때 스택을 많이 쓰므로 기본 8KB -> 16KB
// (주의: 이 줄은 함수를 만들기 때문에 모든 struct 정의보다 아래에 있어야 함)
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// ==========================================================================
// 디스플레이 / 초기화
// ==========================================================================
void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area) {
  if (area->x1 % 2 != 0) area->x1--;
  if (area->y1 % 2 != 0) area->y1--;
  if (area->x2 % 2 == 0) area->x2++;
  if (area->y2 % 2 == 0) area->y2++;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
  lv_disp_flush_ready(disp);
}

void example_increase_lvgl_tick(void *arg) { lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS); }

void IRAM_ATTR onTouchInt() { isPressed = true; }

void setup() {
  Serial.begin(115200);
  Wire.begin(IIC_SDA, IIC_SCL);

  // 저장된 설정 (볼륨, 싱크 보정)
  prefs.begin("asurada", false);
  current_volume = prefs.getInt("vol", DEFAULT_VOLUME);
  if (current_volume < 0 || current_volume > 21) current_volume = DEFAULT_VOLUME;
  target_volume = current_volume;
  sync_trim = prefs.getInt("trim", SYNC_TRIM_MS);
  servo_dps      = prefs.getInt("sv_dps", SERVO_DPS);
  servo_min_us   = prefs.getInt("sv_min2", SERVO_MIN_US);
  servo_turn_deg = prefs.getInt("sv_deg2", (int)SERVO_TURN_DEG);
  Serial.printf("서보: 속도 %d도/초, 각도 %d도, 시작 세기 %d  (시리얼: D숫자=속도, A숫자=각도, M숫자=시작 세기, T=지금 동작, R=config.h 값으로)\n",
                (int)servo_dps, (int)servo_turn_deg, (int)servo_min_us);
  Serial.printf("설정: volume=%d, sync trim=%d ms\n", current_volume, (int)sync_trim);

  SD_MMC.setPins(ASU_SD_CLK, ASU_SD_CMD, ASU_SD_D0);
  if (!SD_MMC.begin("/sdcard", true, false)) {
    Serial.println("SD_MMC mount FAILED (소리 없이 화면만 동작합니다. 카드 삽입/FAT32 확인)");
  } else {
    Serial.printf("SD_MMC OK, size=%llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
  }

  gfx->begin(LCD_SPI_HZ);
  gfx->fillScreen(0x0000);
  gfx->setBrightness(SCREEN_BRIGHT);
  screenWidth = gfx->width();
  screenHeight = gfx->height();

  // ESP-SR 사전 점검: 설정이 틀리면 재부팅 반복 대신 화면에 이유를 띄우고 멈춤
  Serial.printf("PSRAM: %u bytes\n", (unsigned)ESP.getPsramSize());
  if (!psramFound()) {
    fatal_screen("PSRAM not enabled", "PSRAM: OPI PSRAM");
  }
  if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "model")) {
    fatal_screen("No 'model' partition", "Partition: ESP SR 16M");
  }

  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    pmu_online = true;
    pmu.enableBattVoltageMeasure();
    pmu.enableSystemVoltageMeasure();
  }

  // NeoPixel: 안쪽 눈(1, 3) 상시 점등 (이후 눈 깜빡임은 servoTask 가 담당)
  strip.begin();
  strip.setBrightness(150);
  strip.setPixelColor(1, strip.Color(255, 0, 0));
  strip.setPixelColor(3, strip.Color(255, 0, 0));
  strip.show();

  init_onboard_speaker();

  // 요청 큐는 음성인식 콜백이 쓰므로 ESP-SR 보다 먼저 만든다
  audioQ = xQueueCreate(4, sizeof(AudioCmd));
  seqQ = xQueueCreate(8, sizeof(uint8_t));

  // ---------------- 음성인식 (ESP-SR) ----------------
  // 내부 RAM 을 많이 쓰므로 LVGL(화면 버퍼/객체)보다 "먼저" 시작해야 메모리 부족으로 죽지 않음
  Serial.printf("heap before SR: internal=%u psram=%u\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  wake_begin(sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t));        // 호출어 "Hi Asurada" + 명령어 (wake.ino)
  Serial.printf("음성인식 시작 (Hi Asurada -> One ~ Five)  heap internal=%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  servo_setup();

  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW);
  delay(30);
  digitalWrite(TP_RESET, HIGH);
  delay(50);

  pinMode(TP_INT, INPUT_PULLUP);
  touch.setPins(TP_RESET, TP_INT);
  if (touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL)) {
    touch.reset();
    touch.setMaxCoordinates(466, 466);
    touch.setMirrorXY(true, true);
    attachInterrupt(TP_INT, onTouchInt, FALLING);
  } else {
    Serial.println("Touch init FAILED");
  }

  if (qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    imu_online = true;
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_1024DPS, SensorQMI8658::GYR_ODR_896_8Hz, SensorQMI8658::LPF_MODE_3);
    qmi.enableAccelerometer();
    qmi.enableGyroscope();                // 서보 각도 측정용
  } else {
    Serial.println("IMU(QMI8658) init FAILED -> 서보는 시간 방식으로 동작");
  }

  Serial.printf("heap before LVGL: internal=%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  lv_init();

  uint32_t buf_size = screenWidth * screenHeight / 10;
  lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
  lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
  if (!buf1) buf1 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
  if (!buf2) buf2 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
  if (!buf1) {
    Serial.println("LVGL draw buffer alloc FAILED");
    while (1) delay(1000);
  }
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.rounder_cb = example_lvgl_rounder_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  const esp_timer_create_args_t tick_args = { .callback = &example_increase_lvgl_tick, .name = "lvgl_tick" };
  esp_timer_handle_t tick_timer = NULL;
  esp_timer_create(&tick_args, &tick_timer);
  esp_timer_start_periodic(tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

  load_images();           // 펌웨어에 들어 있는 배경/구슬 그림을 PSRAM 으로 풀기 (create_ui 보다 먼저)
  create_ui();
  last_act_ms = millis();
  lv_timer_create(main_timer_cb, 16, NULL);          // 16ms 마다 화면 갱신 확인
  lv_timer_create(gyro_timer_cb, 5, NULL);           // 서보가 움직일 때만 자이로를 읽음

  // ---------------- Task 생성 ----------------
  //                         함수        이름     스택   인자 우선순위 핸들 코어
  xTaskCreatePinnedToCore(audioTask, "audio", 12288, NULL, 6, NULL, 0);   // 음성인식(ESP-SR) Task 보다 높게
  xTaskCreatePinnedToCore(seqTask,   "seq",    6144, NULL, 7, NULL, 0);   // 시퀀스는 가장 높게
  xTaskCreatePinnedToCore(servoTask, "servo",  6144, NULL, 1, NULL, 0);

  audio_play_dir(SND_BOOT);    // 부팅 음성
  vTaskPrioritySet(NULL, 6);   // loop(LVGL 화면) Task 우선순위 1 -> 6 : 음성인식(우선순위 5)에 밀려 화면이 늦는 것 방지
  Serial.println("Setup done! (asurada v03)");
}

void loop() {
  lv_timer_handler();     // 화면/터치 (core 1). 오디오·서보·시퀀스는 각자 Task 에서 처리
  delay(2);
}
