// ==========================================================
// 하드웨어 보조: 코덱/마이크 초기화, 설정 오류 화면, 서보 + NeoPixel Task
// ==========================================================
void es8311_write_reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static void tca_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t exio_read() {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)TCA9554_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

static void i2c_w(uint8_t dev, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t i2c_r(uint8_t dev, uint8_t reg) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)dev, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

// ES7210 마이크 ADC: 16 kHz, MCLK = 256 x fs = 4.096 MHz 기준 (마이크 테스트에서 동작 확인한 설정)
static void es7210_init() {
  const uint8_t A = ES7210_ADDR;
  i2c_w(A, 0x00, 0xFF); delay(20);
  i2c_w(A, 0x00, 0x32);
  i2c_w(A, 0x01, 0x3F);
  i2c_w(A, 0x09, 0x30);
  i2c_w(A, 0x0A, 0x30);
  i2c_w(A, 0x23, 0x2A);
  i2c_w(A, 0x22, 0x0A);
  i2c_w(A, 0x20, 0x0A);
  i2c_w(A, 0x21, 0x2A);
  i2c_w(A, 0x08, i2c_r(A, 0x08) & ~0x01);   // I2S 슬레이브
  i2c_w(A, 0x40, 0xC3);
  i2c_w(A, 0x41, 0x70);
  i2c_w(A, 0x42, 0x70);
  i2c_w(A, 0x11, 0x60);                     // 16bit I2S
  i2c_w(A, 0x12, 0x00);
  i2c_w(A, 0x02, 0xC1);
  i2c_w(A, 0x07, 0x20);
  i2c_w(A, 0x04, 0x01);
  i2c_w(A, 0x05, 0x00);

  for (uint8_t r = 0x43; r <= 0x46; r++) i2c_w(A, r, i2c_r(A, r) & ~0x10);
  i2c_w(A, 0x4B, 0xFF);
  i2c_w(A, 0x4C, 0xFF);

  i2c_w(A, 0x01, i2c_r(A, 0x01) & ~0x0B);
  i2c_w(A, 0x4B, 0x00);
  for (uint8_t r = 0x43; r <= 0x44; r++) {
    uint8_t v = i2c_r(A, r) | 0x10;
    v = (v & ~0x0F) | (ES7210_MIC_GAIN & 0x0F);
    i2c_w(A, r, v);
  }
  i2c_w(A, 0x47, 0x08); i2c_w(A, 0x48, 0x08);
  i2c_w(A, 0x49, 0x08); i2c_w(A, 0x4A, 0x08);
  i2c_w(A, 0x06, 0x04);
  i2c_w(A, 0x4B, 0x0F); i2c_w(A, 0x4C, 0x0F);
  i2c_w(A, 0x00, 0x71); i2c_w(A, 0x00, 0x41);
}

// 설정이 틀렸을 때: 재부팅 반복 대신 화면/시리얼에 이유를 띄우고 멈춤
static void fatal_screen(const char *l1, const char *l2) {
  Serial.println(l1);
  Serial.println(l2);
  gfx->fillScreen(0x0000);
  gfx->setTextSize(2);
  gfx->setTextColor(0xF800);
  gfx->setCursor(100, 180);
  gfx->print("SETUP ERROR");
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(70, 220);
  gfx->print(l1);
  gfx->setTextColor(0xFFE0);
  gfx->setCursor(70, 250);
  gfx->print(l2);
  while (true) delay(1000);
}

void init_onboard_speaker() {
  pinMode(BOARD_PA_PIN, OUTPUT);
  digitalWrite(BOARD_PA_PIN, LOW);

  Wire.beginTransmission(TCA9554_ADDR);
  if (Wire.endTransmission() == 0) {
    tca_write(0x01, 0x07);
    tca_write(0x03, 0xF8);
  }

  // I2S 를 먼저 시작 (MCLK 이 나온 뒤에 코덱 설정). 재생과 마이크(ESP-SR)가 이 I2S 를 같이 씀.
  // setPins 순서: (BCLK, WS, DOUT, DIN, MCLK)
  i2s.setTimeout(1000);
  i2s.setPins(BOARD_I2S_SCLK, BOARD_I2S_LRCK, BOARD_I2S_DOUT, BOARD_I2S_DIN, BOARD_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, I2S_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    fatal_screen("I2S begin failed", "check pins / core ver");
  }
  delay(50);

  es8311_write_reg(0x00, 0x1F);
  delay(10);
  es8311_write_reg(0x00, 0x80);
  es8311_write_reg(0x01, 0x3F);
  es8311_write_reg(0x02, 0x00);
  es8311_write_reg(0x03, 0x10);
  es8311_write_reg(0x04, 0x10);
  es8311_write_reg(0x05, 0x00);
  es8311_write_reg(0x06, 0x03);
  es8311_write_reg(0x07, 0x00);
  es8311_write_reg(0x08, 0xFF);
  es8311_write_reg(0x09, 0x0C);
  es8311_write_reg(0x0A, 0x0C);
  es8311_write_reg(0x0D, 0x01);
  es8311_write_reg(0x0E, 0x02);
  es8311_write_reg(0x12, 0x00);
  es8311_write_reg(0x13, 0x10);
  es8311_write_reg(0x1C, 0x6A);
  es8311_write_reg(0x32, 0xBF);   // DAC 볼륨 0 dB
  es8311_write_reg(0x37, 0x08);

  es7210_init();                  // 마이크 ADC

  digitalWrite(BOARD_PA_PIN, HIGH);
}

// ==========================================================
// [Task] 서보 + NeoPixel 눈
//   SERVO_INTERVAL_MS 마다: 오른쪽으로 돌림 -> 눈 깜빡이며 머묾 -> 처음 자리로 돌아옴
//   NeoPixel(strip)은 이 Task 에서만 건드림 (setup() 의 초기 점등 제외)
//
//   360도(연속회전) 서보는 "각도"가 아니라 "속도"로 움직입니다. 그래서 시간만으로 갔다 오면
//   갈 때와 올 때 돈 양이 달라 조금씩 한쪽으로 밀립니다. SERVO_MODE 1 은 보드의 자이로로
//   실제로 돈 각도를 재서, 돌아올 때 그 각도가 0 이 될 때까지 돌립니다.
// ==========================================================
static void eyes(bool on) {
  uint32_t o = on ? strip.Color(255, 30, 0) : 0;
  uint32_t r = on ? strip.Color(255, 0, 0)  : 0;
  strip.setPixelColor(0, o);   // 입
  strip.setPixelColor(2, r);   // 우측 눈
  strip.setPixelColor(4, r);   // 좌측 눈
  strip.show();
}

// 서보 펄스는 보드의 LEDC(PWM)로 직접 냅니다. 50Hz(20ms 주기)에서 펄스 폭(us)을 듀티로 바꿉니다.
//   (ESP32Servo 라이브러리는 이 보드에서 detach 후 다시 attach 할 때 멈추는 문제가 있어 쓰지 않습니다)
#define SERVO_PWM_BITS 14
static void servo_us(int us) {
  ledcWrite(SERVO_PIN, (uint32_t)(((uint64_t)us << SERVO_PWM_BITS) / 20000ULL));
}

static void servo_deg(int deg) {     // 각도 서보: 0 ~ 180도 = 500 ~ 2400us
  servo_us(500 + (deg * (2400 - 500)) / 180);
}

static void servo_rest() {           // 정지 후 펄스를 끔 (360도 서보가 혼자 조금씩 도는 것 방지)
#if SERVO_MODE != 2
  servo_us(SERVO_STOP_US);
#endif
#if SERVO_DETACH_IDLE
  vTaskDelay(pdMS_TO_TICKS(120));
  ledcWrite(SERVO_PIN, 0);           // 펄스 없음. 다음에 servo_us() 를 부르면 다시 나옴
#endif
}

void servo_setup() {
  if (!ledcAttach(SERVO_PIN, 50, SERVO_PWM_BITS)) Serial.println("서보 PWM 설정 실패");
#if SERVO_MODE == 2
  servo_deg(SERVO_CENTER_DEG);
  delay(300);
#else
  servo_us(SERVO_STOP_US);
#endif
#if SERVO_DETACH_IDLE
  delay(60);
  ledcWrite(SERVO_PIN, 0);
#endif
}

// 머무는 동안 0.5초 간격으로 눈 깜빡임
static void hold_blink() {
  uint32_t t0 = millis();
  int last = -1;
  while (millis() - t0 < SERVO_HOLD_MS) {
    int k = (int)((millis() - t0) / 500);
    if (k != last) { last = k; eyes(k % 2 == 0); }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---- 자이로 (각도 측정) ----
// I2C 는 loop Task 에서만 쓰기로 되어 있으므로, 자이로도 loop Task 의 타이머(gyro_timer_cb)가 읽고 각도를 쌓습니다.
// 이 Task 는 켜고 끄는 신호(gyro_cmd)만 주고 결과(gyro_ang)만 읽습니다.
// (서보 Task 에서 직접 읽었더니 터치/PWR 버튼 읽기와 부딪혀 값이 깨졌음 - 2026-11-23 실기 확인)
static bool gyro_begin() {               // 영점을 재고 각도를 0 으로. 0.8초 안에 준비되면 true
  if (!imu_online) return false;
  gyro_cmd = 1;
  uint32_t t0 = millis();
  while (gyro_state != 2 && millis() - t0 < 800) vTaskDelay(pdMS_TO_TICKS(10));
  return gyro_state == 2;
}

static void gyro_end() { gyro_cmd = 0; }

static int gyro_main_axis() {            // 가장 많이 돈 축
  int a = 0;
  for (int i = 1; i < 3; i++) if (fabsf(gyro_ang[i]) > fabsf(gyro_ang[a])) a = i;
  return a;
}

// 모드 0: 시간으로 갔다 오기 (원래 방식)
static void servo_cycle_timed() {
  eyes(true);
  servo_us(SERVO_RIGHT_US);
  vTaskDelay(pdMS_TO_TICKS(SERVO_RIGHT_MS));
  servo_us(SERVO_STOP_US);
  hold_blink();
  eyes(false);
  servo_us(SERVO_LEFT_US);
  vTaskDelay(pdMS_TO_TICKS(SERVO_LEFT_MS));
  servo_us(SERVO_STOP_US);
}

// 모드 1: 자이로로 각도를 재면서 갔다 오기
//   u > 0 = 갈 때 방향, u < 0 = 돌아올 방향. |u| = 정지 펄스에서 벗어난 양(us) = 속도
static void servo_drive(int u) {
  servo_us(SERVO_STOP_US + SERVO_RIGHT_SIGN * u);
}

// 느리게 움직이기: SERVO_ON_MS 동안 돌리고 off_ms 동안 멈추기를 반복한다.
//   이 모터(SG90 360)는 일정 세기 아래에서는 아예 안 돌고, 돌기 시작하면 바로 초당 70도쯤으로 돈다.
//   펄스 폭으로는 더 느리게 만들 수 없어서, 돌리는 시간의 비율로 평균 속도를 낮춘다. (2026-11-23 실기 측정)
static void servo_drive_slow(int u, uint32_t el_ms, int off_ms) {
  uint32_t per = (uint32_t)SERVO_ON_MS + (uint32_t)(off_ms > 0 ? off_ms : 0);
  if (off_ms <= 0 || (el_ms % per) < (uint32_t)SERVO_ON_MS) servo_drive(u);
  else servo_us(SERVO_STOP_US);
}

// 속도 맞추기: 0.25초마다 실제로 돈 속도(도/초)를 재서, 목표 속도가 되도록 쉬는 시간을 고친다.
//   갈 때와 올 때 모터 힘이 달라도 같은 속도가 된다. 안 움직이면 세기를, 쉬지 않아도 느리면 세기를 올린다.
//   moved_deg = 지난 구간에 돈 각도, dt_ms = 구간 길이, slow = 목표 근처(절반 속도)
static void servo_regulate(float moved_deg, uint32_t dt_ms, bool slow, float &off_ms, int &u, int u_cap) {
  float want = (float)servo_dps * (slow ? 0.5f : 1.0f);
  float dps = moved_deg * 1000.0f / (float)dt_ms;
  if (dps < 1.0f) {                                  // 안 움직임 -> 더 세게
    if (u < u_cap) u += 4;
    return;
  }
  float k = dps / want;                              // 1 보다 크면 너무 빠름 -> 더 쉬게
  if (k < 0.6f) k = 0.6f;
  if (k > 1.6f) k = 1.6f;
  off_ms = (off_ms + 10.0f) * k - 10.0f;
  if (off_ms > 300.0f) off_ms = 300.0f;
  if (off_ms < 0.0f) {
    off_ms = 0.0f;
    if (dps < want * 0.7f && u < u_cap) u += 3;      // 쉬지 않아도 느림 -> 더 세게
  }
}

static void servo_cycle_gyro() {
  // 모터마다 "이 세기부터 움직인다"는 값이 달라서, 한 번 알아낸 값은 다음 동작에도 씁니다
  static int   min_out = SERVO_MIN_US, min_back = SERVO_MIN_US;
  static float off_ms[2] = {SERVO_OFF_MS, SERVO_OFF_MS};   // 쉬는 시간: [0] = 갈 때 방향, [1] = 돌아올 방향 (속도를 재면서 배움)
  static float ms_per_deg[2] = {10.0f, 10.0f};         // 미세 조정: [0] = 갈 때 방향, [1] = 돌아올 방향
  static int   nudge_u[2] = {SERVO_MIN_US + 6, SERVO_MIN_US + 6};
  static bool  first = true;
  if (first || servo_relearn) {                        // 처음이거나 조정값이 바뀜 -> 다시 배움
    first = false;
    servo_relearn = false;
    min_out = min_back = servo_min_us;
    nudge_u[0] = nudge_u[1] = servo_min_us + 4;
    ms_per_deg[0] = ms_per_deg[1] = 10.0f;
    off_ms[0] = off_ms[1] = SERVO_OFF_MS;
  }
  const float turn_deg = (float)servo_turn_deg;
  const int   u_cap = servo_min_us + 100;              // 세기를 올릴 수 있는 한도

  if (!gyro_begin()) {                                 // 센서를 못 읽으면 시간 방식
    gyro_end();
    Serial.println("[SERVO] 자이로를 읽지 못함 -> 시간 방식으로 동작");
    servo_cycle_timed();
    return;
  }
  eyes(true);

  // 1) 갈 때: 목표 각도까지. 안 움직이면(모터의 불감대) 최소 세기를 조금씩 올림
  uint32_t t0 = millis(), chk_ms = t0, last_ms = t0;
  float chk_mag = 0.0f, mag = 0.0f;
  float u_sum = 0.0f;                                  // 세기 x 시간 (각도를 못 잴 때 되돌릴 양)
  bool measured = true;
  for (;;) {
    uint32_t now = millis();
    mag = fabsf(gyro_ang[gyro_main_axis()]);
    float rem = turn_deg - mag;
    if (rem <= SERVO_STOP_DEG) break;
    if (now - chk_ms >= 250) {                         // 속도를 재서 맞춤
      servo_regulate(mag - chk_mag, now - chk_ms, rem < SERVO_SLOW_DEG, off_ms[0], min_out, u_cap);
      chk_ms = now;
      chk_mag = mag;
    }
    if (now - t0 >= 2500 && mag < SERVO_MIN_DEG) { measured = false; break; }   // 보드가 같이 돌지 않음
    if (now - t0 >= (uint32_t)SERVO_MAX_MS) break;
    int u = min_out;
    servo_drive_slow(u, now - t0, (int)off_ms[0]);
    u_sum += (float)u * (float)(now - last_ms);
    last_ms = now;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  servo_us(SERVO_STOP_US);
  uint32_t out_ms = millis() - t0;

  if (!measured) {                                     // 각도를 못 재면: 간 만큼(세기 x 시간)을 시간으로 되돌림
    gyro_end();
    Serial.println("[SERVO] 회전이 측정되지 않음 (보드가 머리와 함께 도는지 확인). 시간으로 되돌립니다");
    hold_blink();
    eyes(false);
    servo_drive(-min_out);
    vTaskDelay(pdMS_TO_TICKS((uint32_t)(u_sum / (float)min_out)));
    servo_us(SERVO_STOP_US);
    return;
  }

  // 2) 머무는 동안에도 계속 잰다 (멈춘 뒤 더 돈 양, 혼자 밀린 양까지 포함)
  hold_blink();
  eyes(false);
  int ax = gyro_main_axis();
  float sign = (gyro_ang[ax] >= 0.0f) ? 1.0f : -1.0f;
  float went = gyro_ang[ax] * sign;

  // 3) 돌아올 때: 0 에 가까워질수록 느리게, 조금 못 미친 곳에서 멈춤 (멈춘 뒤 관성으로 더 도는 만큼)
  t0 = millis();
  chk_ms = t0;
  float chk_rem = went;
  bool wrong_dir = false;
  for (;;) {
    uint32_t now = millis();
    float rem = gyro_ang[ax] * sign;                   // 남은 각도 (+ = 아직 덜 돌아옴, - = 지나침)
    if (rem <= SERVO_LEAD_DEG) break;
    if (now - chk_ms >= 250) {
      servo_regulate(chk_rem - rem, now - chk_ms, rem < SERVO_SLOW_DEG, off_ms[1], min_back, u_cap);
      chk_ms = now;
      chk_rem = rem;
    }
    if (rem > went + 10.0f) { wrong_dir = true; break; }     // 반대로 돌고 있음 -> 중단
    if (now - t0 >= (uint32_t)SERVO_MAX_MS) break;
    servo_drive_slow(-min_back, now - t0, (int)off_ms[1]);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  servo_us(SERVO_STOP_US);
  uint32_t back_ms = millis() - t0;
  vTaskDelay(pdMS_TO_TICKS(250));                      // 완전히 멈출 때까지 기다림
  float first_err = gyro_ang[ax] * sign;

  // 4) 미세 조정: 남은 오차만큼 아주 짧게 밀고 -> 멈추고 -> 다시 재기를 반복
  //    (연속으로 돌리면 0 근처에서 좌우로 헤매므로 톡톡 밀어서 맞춤. 밀어 본 결과로 다음 길이를 배움)
  int nudges = 0;
  while (!wrong_dir && nudges < SERVO_NUDGE_MAX) {
    float rem = gyro_ang[ax] * sign;
    if (fabsf(rem) <= SERVO_STOP_DEG) break;
    int d = (rem > 0.0f) ? 1 : 0;                      // 덜 돌아왔으면 돌아올 방향으로
    float ms = fabsf(rem) * ms_per_deg[d];
    if (ms < 10.0f) ms = 10.0f;
    if (ms > 90.0f) ms = 90.0f;
    servo_drive(d ? -nudge_u[d] : nudge_u[d]);
    vTaskDelay(pdMS_TO_TICKS((uint32_t)ms));
    servo_us(SERVO_STOP_US);
    vTaskDelay(pdMS_TO_TICKS(200));
    float moved = fabsf(gyro_ang[ax] * sign - rem);
    if (moved < 0.25f) {                               // 안 움직임 -> 조금 더 세게
      if (nudge_u[d] < u_cap) nudge_u[d] += 6;
    } else {                                           // 움직인 양으로 "1도에 몇 ms" 를 고침
      float k = fabsf(rem) / moved;
      if (k < 0.5f) k = 0.5f;
      if (k > 1.6f) k = 1.6f;
      ms_per_deg[d] *= k;
      if (ms_per_deg[d] < 2.0f) ms_per_deg[d] = 2.0f;
      if (ms_per_deg[d] > 40.0f) ms_per_deg[d] = 40.0f;
    }
    nudges++;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  float left = gyro_ang[ax] * sign;
  uint32_t samples = gyro_samples;
  gyro_end();
  Serial.printf("[SERVO] 간 각도 %.1f도 (%u ms, 세기 %d, 쉼 %d ms) / 돌아옴 %u ms (세기 %d, 쉼 %d ms), 오차 %.1f도 -> 미세 조정 %d회 뒤 %.1f도 / 축 %d, 측정 %u회%s\n",
                went, (unsigned)out_ms, min_out, (int)off_ms[0], (unsigned)back_ms, min_back, (int)off_ms[1], first_err, nudges, left, ax, (unsigned)samples,
                wrong_dir ? " / 반대로 돌아서 중단함 (SERVO_RIGHT_SIGN 확인)" : "");
}

// 모드 2: 각도 서보
static void servo_cycle_pos() {
  eyes(true);
  int step = (SERVO_SIDE_DEG >= SERVO_CENTER_DEG) ? 1 : -1;
  for (int a = SERVO_CENTER_DEG; a != SERVO_SIDE_DEG; a += step) { servo_deg(a); vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_MS)); }
  servo_deg(SERVO_SIDE_DEG);
  hold_blink();
  eyes(false);
  for (int a = SERVO_SIDE_DEG; a != SERVO_CENTER_DEG; a -= step) { servo_deg(a); vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_MS)); }
  servo_deg(SERVO_CENTER_DEG);
  vTaskDelay(pdMS_TO_TICKS(300));
}

void servoTask(void *) {
  for (;;) {
    uint32_t w0 = millis();                              // 다음 동작까지 기다림 (시리얼 'T' 가 오면 바로)
    while (millis() - w0 < (uint32_t)SERVO_INTERVAL_MS && !servo_trigger) vTaskDelay(pdMS_TO_TICKS(50));
    servo_trigger = false;
    while (seq_phase != SC_STANDBY || seq_auto) vTaskDelay(pdMS_TO_TICKS(200));   // 부스터 화면 중에는 끝날 때까지 기다림
#if SERVO_MODE == 2
    servo_cycle_pos();
#elif SERVO_MODE == 1
    servo_cycle_gyro();
#else
    servo_cycle_timed();
#endif
    servo_rest();
  }
}
