// ==========================================================
// 속도계: 큰 숫자가 0 -> SPEED_MAX_NUM(200) 으로 올라감 + 좌우 가장자리 막대가 음량계처럼 흔들림
//   막대 하나 = lv_arc 하나 (가는 선 = 배경 호, 굵은 덩어리 = 표시 호)
// ==========================================================
static lv_obj_t *mk_bar_arc(lv_obj_t *parent, int a0, int a1, uint32_t color) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_set_size(a, 452, 452);
  lv_obj_center(a);
  lv_arc_set_rotation(a, 0);
  lv_arc_set_bg_angles(a, a0, a1);
  lv_obj_remove_style(a, NULL, LV_PART_KNOB);
  lv_obj_set_style_arc_width(a, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);
  no_scroll(a);
  return a;
}

// i 번째 막대의 굵은 덩어리 길이(도)를 반영
static void speed_bar_apply(int i, int len) {
  if (len == sp_bar_shown[i]) return;
  sp_bar_shown[i] = len;
  if (i == 0)      lv_arc_set_angles(sp_arc[0], 205 - len, 205);   // 왼쪽 하늘색: 아래쪽 끝에서 위로 자람
  else if (i == 1) lv_arc_set_angles(sp_arc[1], 358 - len, 358);   // 오른쪽 초록: 3시에서 위로
  else             lv_arc_set_angles(sp_arc[2], 2, 2 + len);       // 오른쪽 빨강: 3시에서 아래로
}

static void create_speed_ui(lv_obj_t *scr) {
  speed_cont = mk_cont(scr);

  sp_arc[0] = mk_bar_arc(speed_cont, 150, 212, COL_CYAN);
  sp_arc[1] = mk_bar_arc(speed_cont, 320, 358, COL_GREEN);
  sp_arc[2] = mk_bar_arc(speed_cont, 2, 24, COL_RED);
  for (int i = 0; i < 3; i++) speed_bar_apply(i, (int)sp_bar[i]);

  // 세 자리 숫자, 가운데 정렬 위치에서 오른쪽 정렬 (자리가 줄면 오른쪽에 남음)
  segnum_create(sp_num, speed_cont, 233 + SEG_SP_ADV, 233, &SEG_SP_O[0][0][0], NULL, SEG_SP_W, SEG_SP_H, SEG_SP_ADV, 3);
  segnum_force(sp_num, 0);
}

static void speed_start() {
  segnum_force(sp_num, 0);
  sp_bar_ms = 0;
}

static void speed_update(uint32_t now, uint32_t el) {
  int v = (int)(((uint64_t)el * SPEED_MAX_NUM) / SPEED_MS);
  if (v > SPEED_MAX_NUM) v = SPEED_MAX_NUM;
  segnum_set(sp_num, v, now);
  segnum_tick(sp_num, now);

  // 막대: 90ms 마다 새 목표 길이를 정하고, 매 프레임 목표 쪽으로 조금씩 움직임
  static const uint8_t LO[3] = {10, 6, 6};
  static const uint8_t HI[3] = {46, 24, 20};
  if ((now - sp_bar_ms) >= 90) {
    sp_bar_ms = now;
    for (int i = 0; i < 3; i++) sp_bar_to[i] = LO[i] + (float)(esp_random() % (uint32_t)(HI[i] - LO[i] + 1));
  }
  for (int i = 0; i < 3; i++) {
    sp_bar[i] += (sp_bar_to[i] - sp_bar[i]) * 0.35f;
    speed_bar_apply(i, (int)lroundf(sp_bar[i]));
  }
}
