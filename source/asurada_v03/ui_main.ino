// ==========================================================
// 화면 전환 / 터치 / 화면 보호 / 설정 저장  (모두 loop Task 의 LVGL 타이머에서 실행)
//   - 여기서만 LVGL 함수와 터치·PWR 버튼 I2C 를 사용합니다.
//   - 단계 전환은 seqTask 가 하고, 여기서는 seq_phase 를 보고 그리기만 합니다.
// ==========================================================
void create_ui(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  no_scroll(scr);

  create_standby_ui(scr);
  create_speed_ui(scr);
  create_boost1_ui(scr);
  create_boost2_ui(scr);

  show_screen(SC_STANDBY);
  standby_enter(true, millis());        // 부팅 때도 돌면서 등장
}

// id: SC_STANDBY / SC_SPEED / SC_FAN / SC_CIRC, 그 밖(SC_BLACK) = 전부 숨김(검은 화면)
static void show_screen(int id) {
  lv_obj_t *L[4] = {standby_cont, speed_cont, boost1_cont, boost2_cont};
  for (int i = 0; i < 4; i++) {
    if (i == id) lv_obj_clear_flag(L[i], LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(L[i], LV_OBJ_FLAG_HIDDEN);
  }
}

// 1단 -> 2단 전환: 2단 화면이 오른쪽에서 들어오고 1단 화면은 왼쪽으로 밀려 나감
//   LVGL 로 화면 두 장을 움직이면 한 장면을 그리는 데 오래 걸려 뚝뚝 끊깁니다.
//   그래서 미리 만들어 둔 두 장(SLIDE_A = 1단의 마지막 모습, SLIDE_B = 2단의 처음 모습, assets.h 의 압축 그림)을
//   한 줄씩 풀어 이어 붙여 화면에 바로 보냅니다. 이 동안(B2_SLIDE_MS)은 LVGL 이 쉬고, 끝나면 2단 화면을 이어서 그립니다.
//   (PSRAM 에 풀어 두고 읽는 것보다 플래시의 압축 그림을 바로 푸는 쪽이 빠름 - 2026-11-23 실기 측정)
static int ui_prev_ph = -1;               // 직전에 보여 준 화면

// 한 번에 보낼 줄 수: 디스플레이 라이브러리가 한 번에 보내는 픽셀 수(build_opt.h)에 맞춤
#define SLIDE_ROWS ((ESP32QSPI_MAX_PIXELS_AT_ONCE / ASSET_W) > 0 ? (ESP32QSPI_MAX_PIXELS_AT_ONCE / ASSET_W) : 1)
static uint16_t slide_chunk[SLIDE_ROWS * ASSET_W] __attribute__((aligned(4)));   // 내부 RAM (그대로 전송 가능한 메모리)

// 줄 하나(RLE)에서 skip 픽셀을 건너뛰고 n 픽셀을 dst 에 푼다.
//   값은 디스플레이가 받는 바이트 순서(상위 바이트 먼저)로 미리 뒤집어 넣는다 -> writeBytes 로 그대로 보낼 수 있다.
//   (writePixels 는 픽셀마다 뒤집는 계산을 해서 한 장면에 약 28ms 가 더 걸렸음 - 2026-11-23 실기 측정)
static void rle_row_span(const uint16_t *p, int skip, int n, uint16_t *dst) {
  while (n > 0) {
    int cnt = p[0];
    uint16_t v = p[1];
    v = (uint16_t)((v >> 8) | (v << 8));
    p += 2;
    if (skip >= cnt) { skip -= cnt; continue; }
    cnt -= skip;
    skip = 0;
    if (cnt > n) cnt = n;
    n -= cnt;
    while (cnt--) *dst++ = v;
  }
}

static void slide_direct(uint32_t start_ms) {
  const int W = ASSET_W;
  uint32_t t0 = millis();
  uint32_t us_decode = 0, us_send = 0;               // 그림 푸는 시간 / 화면으로 보내는 시간 (측정용)
  int frames = 0;
  for (;;) {
    int32_t el = (int32_t)(millis() - start_ms);     // 목표 시작 시각 기준 (늦게 시작해도 끝나는 시각은 같음)
    if (el < 0) el = 0;
    float t = (el >= (int32_t)B2_SLIDE_MS) ? 1.0f : (float)el / (float)B2_SLIDE_MS;
    int dx = (int)lroundf((float)W * ease_in_out_cubic(t));      // 지금까지 밀린 픽셀 수
    if (dx < 0) dx = 0;
    if (dx > W) dx = W;

    gfx->startWrite();
    gfx->writeAddrWindow(0, 0, W, W);
    for (int row = 0; row < W; row += SLIDE_ROWS) {
      int nr = (W - row < SLIDE_ROWS) ? (W - row) : SLIDE_ROWS;
      uint32_t u0 = micros();
      for (int r = 0; r < nr; r++) {                 // 한 줄 = 1단 그림의 오른쪽 부분 + 2단 그림의 왼쪽 부분
        uint16_t *d = slide_chunk + (size_t)r * W;
        int y = row + r;
        if (dx < W) rle_row_span(SLIDE_A_RLE + SLIDE_A_ROW[y], dx, W - dx, d);
        if (dx > 0) rle_row_span(SLIDE_B_RLE + SLIDE_B_ROW[y], 0, dx, d + (W - dx));
      }
      uint32_t u1 = micros();
      bus->writeBytes((uint8_t *)slide_chunk, (uint32_t)nr * W * 2);
      us_decode += u1 - u0;
      us_send += micros() - u1;
    }
    gfx->endWrite();

    frames++;
    if (t >= 1.0f) break;
  }
  Serial.printf("[SLIDE] %d 장면 / %u ms (장면당 그림 풀기 %u us, 화면 전송 %u us)\n", frames, (unsigned)(millis() - t0),
                (unsigned)(us_decode / frames), (unsigned)(us_send / frames));
}

// 단계가 바뀌었을 때 화면을 한 번 초기화
static void ui_enter_phase(int ph, uint32_t now) {
  switch (ph) {
    case SC_STANDBY:
      show_screen(SC_STANDBY);
      standby_enter(true, now);
      break;
    case SC_SPEED:
      speed_start();
      show_screen(SC_SPEED);
      break;
    case SC_FAN:
      b1_start();
      show_screen(SC_FAN);
      break;
    case SC_CIRC:
      b2_start();
      if (ui_prev_ph == SC_FAN) slide_direct(phase_ms);   // 1단에서 넘어올 때만 밀고 들어옴
      show_screen(SC_CIRC);
      break;
    default:
      show_screen(-1);                  // 검은 화면
      break;
  }
  ui_prev_ph = ph;
  lv_obj_invalidate(lv_scr_act());
}

// 화면 보호: 대기화면에서 입력이 없으면 어둡게 / (설정 시) 끔
static void care_tick(uint32_t now, bool active) {
  if (active) last_act_ms = now;
  uint32_t idle = now - last_act_ms;
  int want = SCREEN_BRIGHT;
#if DIM_AFTER_MS > 0
  if (idle >= (uint32_t)DIM_AFTER_MS) want = DIM_LEVEL;
#endif
#if OFF_AFTER_MS > 0
  if (idle >= (uint32_t)OFF_AFTER_MS) want = 0;
#endif
  if (want != screen_level) {
    screen_level = want;
    gfx->setBrightness((uint8_t)want);
  }
}

// 볼륨/싱크 값이 바뀌고 2초 지나면 저장 (플래시를 자주 쓰지 않도록)
static void settings_tick(uint32_t now) {
  static uint32_t dirty_ms = 0;
  static bool pending = false;
  if (cfg_dirty) {
    cfg_dirty = false;
    pending = true;
    dirty_ms = now;
  }
  if (pending && (now - dirty_ms) >= 2000) {
    pending = false;
    prefs.putInt("vol", current_volume);
    prefs.putInt("trim", (int)sync_trim);
    prefs.putInt("sv_dps", (int)servo_dps);
    prefs.putInt("sv_min2", (int)servo_min_us);
    prefs.putInt("sv_deg2", (int)servo_turn_deg);
    Serial.printf("설정 저장: volume=%d, sync trim=%d ms\n", current_volume, (int)sync_trim);
  }
}

// 자이로 읽기 (서보가 움직이는 동안만). 각속도(도/초)를 시간으로 쌓아 각도로 만든다
void gyro_timer_cb(lv_timer_t *timer) {
  static float bias[3], sum[3];
  static int n = 0;
  static uint32_t t0_ms = 0, last_us = 0;
  if (!gyro_cmd) { gyro_state = 0; return; }
  if (!imu_online) return;
  float g[3];
  if (!qmi.getGyroscope(g[0], g[1], g[2])) return;
  uint32_t now_us = micros();
  if (gyro_state == 0) {                               // 켜짐: 영점 재기 시작
    sum[0] = sum[1] = sum[2] = 0.0f;
    n = 0;
    t0_ms = millis();
    gyro_state = 1;
  }
  if (gyro_state == 1) {                               // 가만히 있는 0.25초 동안의 평균 = 영점
    for (int i = 0; i < 3; i++) sum[i] += g[i];
    n++;
    if (millis() - t0_ms >= 250 && n >= 10) {
      for (int i = 0; i < 3; i++) { bias[i] = sum[i] / n; gyro_ang[i] = 0.0f; }
      gyro_samples = 0;
      last_us = now_us;
      gyro_state = 2;
    }
    return;
  }
  float dt = (float)(now_us - last_us) * 1e-6f;
  last_us = now_us;
  if (dt > 0.25f) dt = 0.25f;
  for (int i = 0; i < 3; i++) gyro_ang[i] = gyro_ang[i] + (g[i] - bias[i]) * dt;
  gyro_samples = gyro_samples + 1;
}

void main_timer_cb(lv_timer_t *timer) {
  uint32_t now = millis();
  bool active = false;

  // ---------------- 터치 ----------------
  static uint8_t touch_miss = 0;
  bool poll = isPressed || is_touching || digitalRead(TP_INT) == LOW;
  if (now < (uint32_t)TOUCH_IGNORE_BOOT_MS) {       // 켜진 직후의 터치 신호는 읽어서 버림
    if (poll) { isPressed = false; touch.getPoint(x, y, touch.getSupportTouchPoint()); }
    poll = false;
  }
  if (poll) {
    isPressed = false;
    uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
    if (touched > 0) {
      touch_miss = 0;
      last_touch_ms = now;
      active = true;
      if (!is_touching) {
        is_touching = true;
        press_start_ms = now;
        long_press_handled = false;
        is_swipe = false;
        start_x = x[0];
        start_y = y[0];
        if (screen_level == 0) {
          touch_consumed = true;                    // 꺼진 화면을 깨우는 터치는 다른 동작을 하지 않음
        } else {
          touch_consumed = (seq_bgm != 0);          // 배경음악 재생 중에는 터치 한 번에 바로 종료
          if (touch_consumed) seq_post(REQ_STOP);
        }
      } else if (!touch_consumed) {
        int16_t dx = x[0] - start_x;
        int16_t dy = y[0] - start_y;

        // 좌우 스와이프: 오른쪽 = 배경음악 A, 왼쪽 = 배경음악 B (대기화면에서만)
        if (!is_swipe && abs(dx) > BGM_SWIPE_MIN_PX && abs(dx) > abs(dy)) {
          is_swipe = true;
          if (seq_phase == SC_STANDBY && !seq_auto) {
            bool right = (dx > 0);
            if (BGM_SWIPE_INVERT) right = !right;
            seq_post(right ? REQ_BGM7 : REQ_BGM8);
          }
        }
        // 위/아래 스와이프 = 볼륨
        else if (!is_swipe && abs(dy) > 50 && abs(dy) > abs(dx)) {
          is_swipe = true;
          current_volume += (dy < 0) ? 2 : -2;
          if (current_volume > 21) current_volume = 21;
          if (current_volume < 0) current_volume = 0;
          target_volume = current_volume;         // audioTask 가 적용
          cfg_dirty = true;
          Serial.printf("Volume: %d\n", current_volume);
        }

        // 대기화면에서 길게 터치 -> 전체 시퀀스 요청
        if (!long_press_handled && !is_swipe && seq_phase == SC_STANDBY && !seq_auto &&
            (now - press_start_ms >= (uint32_t)LONG_PRESS_MS)) {
          long_press_handled = true;
          seq_post(REQ_LONG);
        }
      }
    } else if (is_touching && ++touch_miss >= 3) {
      touch_miss = 0;
      is_touching = false;
      if (!touch_consumed && !long_press_handled && !is_swipe && (last_touch_ms - press_start_ms < 500)) {
        seq_post(REQ_TAP);                        // 짧은 탭
      }
      touch_consumed = false;
    }
  }

  // ---------------- 화면 그리기 (seq_phase 기준) ----------------
  static int shown_phase = -1;
  static uint32_t shown_pms = 0;
  int ph = seq_phase;
  uint32_t pms = phase_ms;
  if (ph != shown_phase || (pms != shown_pms && ph != SC_STANDBY)) {   // 단계가 바뀌거나 같은 단계가 다시 시작되면 화면 초기화
    bool first = (shown_phase == -1);
    shown_phase = ph;
    shown_pms = pms;
    if (!first) ui_enter_phase(ph, now);          // 부팅 직후의 대기화면은 create_ui 가 이미 준비함
  }

  // 음성인식 상태 표시 ("Hi Asurada" 를 들으면 LISTENING)
  static bool shown_listen = false;
  bool want_listen = (g_ui == UI_LISTEN);
  if (want_listen != shown_listen) {
    shown_listen = want_listen;
    if (want_listen) lv_obj_clear_flag(listen_lbl, LV_OBJ_FLAG_HIDDEN);
    else             lv_obj_add_flag(listen_lbl, LV_OBJ_FLAG_HIDDEN);
  }

  uint32_t e = now - pms;
  if ((int32_t)e < 0) e = 0;                      // 단계 시작 시각이 미래일 때(소리 지연 보정)

  switch (ph) {
    case SC_SPEED:   speed_update(now, e); break;
    case SC_FAN:     b1_update(now);       break;
    case SC_CIRC:    b2_update(now);       break;
    case SC_STANDBY: standby_update(now);  break;
    default: break;
  }

  // ---------------- PWR 버튼 ----------------
  static bool prev_pwr = false;
  static uint8_t pwr_cnt = 0;                     // 3번 연속(약 50ms) 눌린 것으로 읽혀야 누름으로 인정
  if ((exio_read() & (1 << EXIO_PWR_BIT)) != 0) { if (pwr_cnt < 3) pwr_cnt++; } else pwr_cnt = 0;
  bool pwr = (pwr_cnt >= 3);
  if (pwr && !prev_pwr) {
    if (screen_level != 0) audio_play_dir(SND_LIFT);   // 화면이 꺼져 있을 때는 깨우기만
  }
  if (pwr) active = true;
  prev_pwr = pwr;

  // ---------------- 화면 보호 / 설정 저장 ----------------
  if (ph != SC_STANDBY || seq_bgm != 0 || want_listen) active = true;
  care_tick(now, active);
  settings_tick(now);
}
