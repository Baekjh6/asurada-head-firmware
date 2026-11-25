// ==========================================================
// [Task] 오디오 (자체 WAV 재생 엔진) + 음성인식 콜백
//   - ESP-SR(음성인식)이 마이크 I2S 를 16kHz 로 계속 쓰기 때문에, 재생도 같은 I2S 로 출력합니다.
//   - WAV 는 읽으면서 16kHz 모노로 변환합니다 (스테레오는 L/R 평균, 44.1kHz 등은 다운샘플).
//   - PCM WAV(16bit) 만 지원합니다. (MP3 는 재생 불가)
//   요청 문자열:  "/폴더/파일.wav" 파일 하나 / "!dir:/4" 폴더의 첫 .wav / "!bgm:/7" 폴더의 첫 .wav 반복 / "!stop"
// ==========================================================
static bool wav_parse(File &f, WavInfo &w) {
  uint8_t h[12];
  if (f.read(h, 12) != 12) return false;
  if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) return false;
  bool fmtOk = false;
  while (f.available()) {
    uint8_t ck[8];
    if (f.read(ck, 8) != 8) break;
    uint32_t sz = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((uint32_t)ck[7] << 24);
    if (memcmp(ck, "fmt ", 4) == 0) {
      uint8_t fm[16];
      if (sz < 16 || f.read(fm, 16) != 16) return false;
      uint16_t tag = fm[0] | (fm[1] << 8);
      w.channels = fm[2] | (fm[3] << 8);
      w.rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | ((uint32_t)fm[7] << 24);
      w.bits = fm[14] | (fm[15] << 8);
      fmtOk = (tag == 1 || tag == 0xFFFE);
      if (sz > 16) f.seek(f.position() + (sz - 16) + (sz & 1));
    } else if (memcmp(ck, "data", 4) == 0) {
      w.dataStart = f.position();
      w.dataLen = sz;
      return fmtOk;
    } else {
      f.seek(f.position() + sz + (sz & 1));
    }
  }
  return false;
}

// 폴더 안에서 처음 발견되는 .wav 파일 경로를 찾음 (숨김파일 제외)
static bool find_first_wav(const char *dir, char *out, size_t n) {
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) { if (d) d.close(); return false; }
  bool found = false;
  File f;
  while ((f = d.openNextFile())) {
    if (!f.isDirectory()) {
      const char *nm = f.name();
      const char *base = strrchr(nm, '/');
      base = base ? base + 1 : nm;
      String low = String(base);
      low.toLowerCase();
      if (base[0] != '.' && low.endsWith(".wav")) {
        snprintf(out, n, "%s/%s", dir, base);
        found = true;
      }
    }
    f.close();
    if (found) break;
  }
  d.close();
  return found;
}

// 어느 Task 에서나 호출 가능 (큐에 넣기만 함)
void audio_play(const char *path) {
  if (!audioQ) return;
  AudioCmd c;
  strlcpy(c.path, path, sizeof(c.path));
  xQueueSend(audioQ, &c, 0);
}

void audio_play_dir(const char *dir) {        // 폴더의 첫 번째 .wav 를 한 번 재생
  char p[48];
  snprintf(p, sizeof(p), "!dir:%s", dir);
  audio_play(p);
}

void audio_stop() { audio_play("!stop"); }

static bool player_open(Player &pl, const char *path, bool loop) {
  pl.active = false;
  if (pl.f) pl.f.close();
  pl.f = SD_MMC.open(path, FILE_READ);
  if (!pl.f) {
    Serial.printf("audio file NOT found: %s\n", path);
    return false;
  }
  if (!wav_parse(pl.f, pl.w)) {
    Serial.printf("WAV 헤더 오류 (PCM WAV 가 아님): %s\n", path);
    pl.f.close();
    return false;
  }
  if (pl.w.bits != 16 || (pl.w.channels != 1 && pl.w.channels != 2) || pl.w.rate < 8000 || pl.w.dataLen < 64) {
    Serial.printf("지원하지 않는 WAV (%u bit, %u ch, %u Hz) : %s\n",
                  (unsigned)pl.w.bits, (unsigned)pl.w.channels, (unsigned)pl.w.rate, path);
    pl.f.close();
    return false;
  }
  pl.f.seek(pl.w.dataStart);
  pl.remain = pl.w.dataLen;
  pl.step = (uint32_t)(((uint64_t)pl.w.rate << 16) / I2S_SAMPLE_RATE);   // 16.16 고정소수점
  pl.pos = 0;
  pl.prev = 0;
  pl.lp = 0;
  // 16kHz 보다 높은 샘플레이트는 약 7kHz 저역통과 후 다운샘플 (소리 갈라짐 방지)
  pl.lp_a = (pl.w.rate > I2S_SAMPLE_RATE)
              ? (int32_t)((1.0f - expf(-6.2832f * 7000.0f / (float)pl.w.rate)) * 65536.0f)
              : 65536;
  pl.loop = loop;
  pl.active = true;
  uint32_t ms = (uint32_t)(((uint64_t)pl.w.dataLen * 1000ULL) / ((uint64_t)pl.w.rate * pl.w.channels * 2));
  Serial.printf("WAV 재생: %s (%u Hz, %u ch, %u.%u 초)\n", path, (unsigned)pl.w.rate, (unsigned)pl.w.channels,
                (unsigned)(ms / 1000), (unsigned)((ms % 1000) / 100));
  return true;
}

// 한 덩어리를 읽어 16kHz 모노로 변환하여 I2S 로 출력. 파일 끝이면 false
static bool player_step(Player &pl, int gain) {      // gain: 0~256
  static int16_t inBuf[1024];                        // 2048 bytes
  static int16_t mono[1026];                         // [0] = 직전 샘플 (보간용)
  static int16_t outBuf[2600];

  if (pl.remain == 0) return false;
  uint32_t want = pl.remain < sizeof(inBuf) ? pl.remain : sizeof(inBuf);
  int len = pl.f.read((uint8_t *)inBuf, want);
  if (len <= 0) return false;
  pl.remain -= len;

  int frames;
  mono[0] = pl.prev;
  if (pl.w.channels == 2) {
    frames = len / 4;
    for (int i = 0; i < frames; i++) {
      int32_t s = ((int32_t)inBuf[i * 2] + inBuf[i * 2 + 1]) / 2;
      pl.lp += (int32_t)(((int64_t)(s - pl.lp) * pl.lp_a) >> 16);
      mono[i + 1] = (int16_t)pl.lp;
    }
  } else {
    frames = len / 2;
    for (int i = 0; i < frames; i++) {
      int32_t s = inBuf[i];
      pl.lp += (int32_t)(((int64_t)(s - pl.lp) * pl.lp_a) >> 16);
      mono[i + 1] = (int16_t)pl.lp;
    }
  }
  if (frames <= 0) return true;
  int total = frames + 1;
  pl.prev = mono[total - 1];

  // 선형 보간으로 16kHz 로 변환 + 볼륨 적용
  int k = 0;
  while (((pl.pos >> 16) + 1) < (uint32_t)total && k < (int)(sizeof(outBuf) / sizeof(int16_t))) {
    uint32_t idx = pl.pos >> 16;
    int32_t frac = pl.pos & 0xFFFF;
    int32_t a = mono[idx], b = mono[idx + 1];
    int32_t v = a + (int32_t)(((int64_t)(b - a) * frac) >> 16);
    outBuf[k++] = (int16_t)((v * gain) >> 8);
    pl.pos += pl.step;
  }
  pl.pos -= ((uint32_t)(total - 1)) << 16;

  i2s.write((uint8_t *)outBuf, k * sizeof(int16_t));
  bool first = (au_samples == 0);
  au_samples += (uint32_t)k;
  au_pos_ms = (uint32_t)(((uint64_t)au_samples * 1000ULL) / I2S_SAMPLE_RATE);
  au_stamp = millis();
  if (first && k > 0) au_gen = au_gen + 1;     // 첫 소리가 나가기 시작함 (맨 마지막에 갱신)
  return true;
}

void audioTask(void *) {
  AudioCmd c;
  static Player pl;
  static char bgm_dir[8] = "";          // 배경음악 폴더 (비어 있으면 BGM 아님)
  static char file[96] = "";
  static int16_t silence[512];
  bool was_active = false;

  for (;;) {
    bool got = false;
    while (xQueueReceive(audioQ, &c, 0) == pdTRUE) got = true;   // 마지막 요청만 처리
    if (got) {
      if (strcmp(c.path, "!stop") == 0) {
        bgm_dir[0] = 0;
        pl.active = false;
      } else if (strncmp(c.path, "!bgm:", 5) == 0) {                // 배경음악: 폴더의 첫 .wav, 끝나면 반복
        strlcpy(bgm_dir, c.path + 5, sizeof(bgm_dir));
        if (find_first_wav(bgm_dir, file, sizeof(file))) {
          isPlayingWav = true;
          if (!player_open(pl, file, true)) { bgm_dir[0] = 0; isPlayingWav = false; seq_bgm = 0; }
        } else {
          Serial.printf("BGM 폴더에 .wav 파일이 없음 (MP3 는 재생 불가): %s\n", bgm_dir);
          bgm_dir[0] = 0;
          seq_bgm = 0;
        }
      } else {                                                     // 효과음 (BGM 반복 해제)
        bgm_dir[0] = 0;
        bool ok;
        if (strncmp(c.path, "!dir:", 5) == 0) {
          ok = find_first_wav(c.path + 5, file, sizeof(file));
          if (!ok) Serial.printf("폴더에 .wav 파일이 없음: %s (소리 없이 진행)\n", c.path + 5);
        } else {
          strlcpy(file, c.path, sizeof(file));
          ok = true;
        }
        if (ok) {
          isPlayingWav = true;
          au_samples = 0;
          ok = player_open(pl, file, false);
        }
        if (!ok) {
          pl.active = false;
          au_fail = au_fail + 1;                                   // seqTask 가 기다리지 않도록 알림
          if (!was_active) isPlayingWav = false;
        }
      }
    }

    if (pl.active) {
      was_active = true;
      int gain = ((int)target_volume * 256) / 21;                  // 볼륨 0~21 -> 0~256
      if (!player_step(pl, gain)) {
        if (pl.loop) {                                             // 배경음악은 처음부터 다시
          pl.f.seek(pl.w.dataStart);
          pl.remain = pl.w.dataLen;
        } else {
          pl.active = false;
        }
      }
    } else {
      if (was_active) {                                            // 재생이 막 끝남: 무음으로 마무리
        was_active = false;
        if (pl.f) pl.f.close();
        memset(silence, 0, sizeof(silence));
        for (int i = 0; i < 4; i++) i2s.write((uint8_t *)silence, sizeof(silence));
        vTaskDelay(pdMS_TO_TICKS(150));
        isPlayingWav = false;
        g_ui = UI_READY;
        ESP_SR.setMode(SR_MODE_WAKEWORD);                          // 재생 중 소리와 상관없이 "Hi ESP" 대기로 복귀
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

// ESP-SR 이벤트 콜백
//   !! ESP-SR 내부 Task 에서 호출됨 -> 화면/SD/I2S 는 건드리지 말고 요청만 남길 것 !!
void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  if (isPlayingWav || seq_auto) return;         // 소리가 나거나 시퀀스 중에는 듣지 않음

  switch (event) {
    case SR_EVENT_WAKEWORD:                       // "Hi ESP"
      g_ui = UI_LISTEN;
      ESP_SR.setMode(SR_MODE_COMMAND);
      break;
    case SR_EVENT_TIMEOUT:
      g_ui = UI_READY;
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    case SR_EVENT_COMMAND:
      sr_last_cmd = command_id;
      sr_last_phrase = phrase_id;
      {
        int id = command_id;
#if SR_SWAP_34
        if (id == CMD_NUM_3) id = CMD_NUM_4; else if (id == CMD_NUM_4) id = CMD_NUM_3;
#endif
        if (id == CMD_NUM_1)      seq_post(REQ_V1);
        else if (id == CMD_NUM_2) seq_post(REQ_V2);
        else if (id == CMD_NUM_3) seq_post(REQ_V3);
        else if (id == CMD_NUM_4) seq_post(REQ_BGM7);
        else if (id == CMD_NUM_5) seq_post(REQ_BGM8);
      }
      g_ui = UI_READY;
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    default:
      break;
  }
}
