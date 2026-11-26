// ==========================================================
// [Task] 시퀀스 (화면 단계 전환 + 소리 요청)
//   화면은 그리지 않음. seq_phase / phase_ms 만 바꾸고, 그리는 건 loop() 가 함.
//
//   시퀀스는 "단계 표"(화면, 길이, 시작할 때 낼 소리)로 정해져 있고, 길이는 config.h 의 시간으로 계산합니다.
//   [터치]  대기에서 터치        -> 속도계 화면 (/2 소리)
//           속도계에서 터치      -> 1단 부스터 (/4 음성) -> 끝나면 대기
//           1단 중에 터치        -> 2단 부스터 (/5 음성): 1단 -> 슬라이드 -> 2단 -> 대기
//           그 밖의 진행 중 터치 -> 중단하고 대기 (/2 소리)
//   [롱터치 3초 / "Three"] 속도계 -> 1단 -> 2단 (/6 음성 하나로 끝까지)
//   ["One"] 1단 (/4)     ["Two"] 1단 -> 2단 (/5)
//   단계 시작 시각은 '실제 시각'이 아니라 '목표 시각'으로 기록하므로 Task 지연이 있어도 밀리지 않습니다.
// ==========================================================
#define B1_FULL_MS  ((uint32_t)B1_STEP_MS * (B1_START_NUM - B1_END_NUM + 1))      // 1단 전체 (기본 14 ~ 6 = 9초)
#define B1_JOIN_MS  (B1_FULL_MS - B2_SLIDE_MS)                                    // 뒤에 2단이 올 때: 마지막에 2단이 밀고 들어오는 시간이 겹침
#define B2_FULL_MS  ((uint32_t)B2_SLIDE_MS + (uint32_t)B2_STEP_MS * B2_START_NUM) // 2단 전체 (슬라이드 + 5 ~ 1)

static const FlowStep FLOW_ONE[] = {
  {SC_FAN,   B1_FULL_MS, SND_B1},
  {SC_BLACK, BLACK_MS,   NULL},
};
static const FlowStep FLOW_TWO[] = {
  {SC_FAN,   B1_JOIN_MS, SND_B12},
  {SC_CIRC,  B2_FULL_MS, NULL},
  {SC_BLACK, BLACK_MS,   NULL},
};
static const FlowStep FLOW_FULL[] = {
  {SC_SPEED, SPEED_MS,   SND_FULL},
  {SC_FAN,   B1_JOIN_MS, NULL},
  {SC_CIRC,  B2_FULL_MS, NULL},
  {SC_BLACK, BLACK_MS,   NULL},
};

// 어느 Task 에서나 호출 가능
void seq_post(uint8_t req) {
  if (seqQ) xQueueSend(seqQ, &req, 0);
}

static void go_phase(int ph, uint32_t start_ms) {
  phase_ms = start_ms;     // 시간을 먼저 쓰고
  seq_phase = ph;          // 단계를 나중에 씀
}

// 지금 스피커에서 나오고 있는 오디오 위치 (ms). 마지막으로 쓴 위치 + 그 후 경과 - 버퍼 지연
static int32_t au_now_ms() {
  uint32_t pos = au_pos_ms;
  uint32_t d = millis() - au_stamp;
  if (d > 100) d = 100;
  return (int32_t)(pos + d) - AUDIO_LATENCY_MS + UI_LEAD_MS + sync_trim;
}

void seqTask(void *) {
  const FlowStep *flow = NULL;
  int n = 0, idx = 0;
  bool wait = false;              // 첫 소리가 실제로 나오길 기다리는 중
  uint32_t gen0 = 0, fail0 = 0, req_ms = 0;
  uint32_t t0 = 0;                // 시퀀스 0초에 해당하는 millis()
  uint32_t cum = 0;               // 현재 단계가 시작되는 시퀀스 시각 (ms)

  auto begin_flow = [&](const FlowStep *f, int cnt) {
    flow = f; n = cnt; idx = 0; cum = 0;
    req_ms = millis();
    t0 = req_ms;
    seq_auto = true;
    go_phase(f[0].ph, req_ms);                      // 화면은 바로 시작 (소리가 나가기 시작하면 다시 맞춤)
    wait = false;
    if (f[0].snd) {
      gen0 = au_gen;                                // audio_play 보다 먼저 읽음
      fail0 = au_fail;
      wait = true;
      audio_play_dir(f[0].snd);
    }
  };
  auto end_flow = [&](bool stop_audio) {
    flow = NULL;
    seq_auto = false;
    if (stop_audio) audio_stop();
    go_phase(SC_STANDBY, millis());
    if (!isPlayingWav) {                            // 시퀀스 중 무시한 음성인식 상태를 처음으로
      g_ui = UI_READY;
      ESP_SR.setMode(SR_MODE_OFF);              // "Hi Asurada" 대기
    }
  };
  auto enter_step = [&](uint32_t start_ms) {        // idx 단계로 들어감
    go_phase(flow[idx].ph, start_ms);
    if (flow[idx].snd) audio_play_dir(flow[idx].snd);
    Serial.printf("[SEQ] 단계 %d (화면 %d)\n", idx, (int)flow[idx].ph);
  };

  int shown_cmd = 0;
  for (;;) {
    wake_report();                                        // 호출어를 들었으면 한 줄 찍는다 (wake.ino)
    if (sr_last_cmd != shown_cmd) {                       // 음성인식 결과 출력 (번호가 안 맞을 때 확인용)
      shown_cmd = sr_last_cmd;
      Serial.printf("[SR] command_id=%d phrase_id=%d\n", shown_cmd, (int)sr_last_phrase);
    }
    while (Serial.available()) {                          // 시리얼 명령
      int ch = Serial.read();
      static char cmd = 0;                                // 숫자를 기다리는 명령 (D / M / A)
      static int num = 0;
      static bool has_num = false;
      if (ch == '+') { sync_trim = sync_trim + 20; cfg_dirty = true; Serial.printf("[SYNC] trim = %d ms (화면 더 빠르게)\n", (int)sync_trim); }
      else if (ch == '-') { sync_trim = sync_trim - 20; cfg_dirty = true; Serial.printf("[SYNC] trim = %d ms (화면 더 느리게)\n", (int)sync_trim); }
      else if (ch == 'T' || ch == 't') { servo_trigger = true; }                    // 서보를 지금 한 번 움직임
      else if (!cmd && ch == '1') { seq_post(REQ_V1); }                             // 시험용: "One" 과 같음 (1단)
      else if (!cmd && ch == '2') { seq_post(REQ_V2); }                             // 시험용: "Two" 와 같음 (1단 + 2단)
      else if (!cmd && ch == '3') { seq_post(REQ_V3); }                             // 시험용: "Three" 와 같음 (속도계 + 1단 + 2단)
      else if (ch == 'R' || ch == 'r') {                                            // 서보 값을 config.h 로 되돌림
        servo_dps = SERVO_DPS; servo_min_us = SERVO_MIN_US; servo_turn_deg = (int)SERVO_TURN_DEG;
        servo_relearn = true; cfg_dirty = true;
        Serial.printf("[SERVO] config.h 값으로: 속도 %d도/초, 각도 %d도, 시작 세기 %d\n", (int)servo_dps, (int)servo_turn_deg, (int)servo_min_us);
      }
      else if (ch == 'D' || ch == 'd' || ch == 'M' || ch == 'm' || ch == 'A' || ch == 'a' || ch == 'W' || ch == 'w') { cmd = (char)toupper(ch); num = 0; has_num = false; }
      else if (cmd && ch >= '0' && ch <= '9') { num = num * 10 + (ch - '0'); has_num = true; }
      else if (cmd) {                                                               // 숫자 끝 (줄바꿈/공백) -> 적용
        if (has_num && cmd == 'W') {                                               // 호출어 판정 기준 (%)
          if (num >= 50 && num <= 99) wake_set_cutoff(num);
        } else if (has_num) {
          if (cmd == 'D' && num >= 3 && num <= 200) servo_dps = num;                // 속도 (도/초)
          if (cmd == 'M' && num >= 5 && num <= 300) { servo_min_us = num; servo_relearn = true; }   // 시작 세기
          if (cmd == 'A' && num >= 5 && num <= 170) servo_turn_deg = num;           // 돌아갈 각도
          cfg_dirty = true;
          Serial.printf("[SERVO] 속도 %d도/초, 각도 %d도, 시작 세기 %d\n", (int)servo_dps, (int)servo_turn_deg, (int)servo_min_us);
        }
        cmd = 0;
      }
    }

    uint8_t r;
    while (seqQ && xQueueReceive(seqQ, &r, 0) == pdTRUE) {
      if (r == REQ_STOP) {                                // 배경음악 중 터치 = 즉시 종료
        if (seq_bgm) {
          seq_bgm = 0;
          audio_stop();
        }
      } else if ((r == REQ_BGM7 || r == REQ_BGM8) && seq_phase == SC_STANDBY && !flow && !seq_bgm) {
        seq_bgm = (r == REQ_BGM7) ? 7 : 8;
        audio_play(r == REQ_BGM7 ? "!bgm:" BGM_DIR_A : "!bgm:" BGM_DIR_B);
      } else if (r == REQ_TAP) {
        if (flow == FLOW_ONE) {                           // 1단 중 터치 -> 2단 부스터 음성 (1단부터 다시 -> 2단)
          begin_flow(FLOW_TWO, sizeof(FLOW_TWO) / sizeof(FlowStep));
        } else if (flow) {                                // 그 밖의 시퀀스 중 터치 -> 중단하고 대기
          end_flow(true);
        } else if (seq_bgm) {
          // 배경음악 중의 터치는 REQ_STOP 으로 처리됨
        } else if (seq_phase == SC_STANDBY) {             // 대기 -> 속도계
          go_phase(SC_SPEED, millis());
          audio_play_dir(SND_MAIN);
        } else if (seq_phase == SC_SPEED) {               // 속도계 -> 1단 부스터
          begin_flow(FLOW_ONE, sizeof(FLOW_ONE) / sizeof(FlowStep));
        } else {
          go_phase(SC_STANDBY, millis());
          audio_play_dir(SND_MAIN);
        }
      } else if (r == REQ_LONG && seq_phase == SC_STANDBY && !flow && !seq_bgm) {
        begin_flow(FLOW_FULL, sizeof(FLOW_FULL) / sizeof(FlowStep));
      } else if ((r == REQ_V1 || r == REQ_V2 || r == REQ_V3) && !flow && !seq_bgm) {
        if (r == REQ_V1)      begin_flow(FLOW_ONE,  sizeof(FLOW_ONE)  / sizeof(FlowStep));
        else if (r == REQ_V2) begin_flow(FLOW_TWO,  sizeof(FLOW_TWO)  / sizeof(FlowStep));
        else                  begin_flow(FLOW_FULL, sizeof(FLOW_FULL) / sizeof(FlowStep));
      }
    }

#if SPEED_HOLD_MS > 0
    // 터치로 연 속도계 화면을 오래 그대로 두면 대기화면으로 (화면 보호)
    if (!flow && seq_phase == SC_SPEED && (millis() - phase_ms) > (uint32_t)SPEED_HOLD_MS && (int32_t)(millis() - phase_ms) > 0) {
      go_phase(SC_STANDBY, millis());
    }
#endif

    if (flow) {
      if (wait) {
        if (au_gen != gen0) {                             // 소리가 실제로 나가기 시작 -> 화면 시계를 소리에 맞춤
          wait = false;
          t0 = millis() - (uint32_t)au_now_ms();
          go_phase(flow[0].ph, t0);
        } else if (au_fail != fail0 || (millis() - req_ms) > 1500) {   // 소리가 없으면 그대로 진행
          wait = false;
        }
      } else {
        int32_t t = (int32_t)(millis() - t0);
        while (flow && t >= (int32_t)(cum + flow[idx].dur)) {
          cum += flow[idx].dur;
          idx++;
          if (idx >= n) {
            end_flow(false);
          } else {
            enter_step(t0 + cum);                         // 목표 시각으로 기록 (밀리지 않게)
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
