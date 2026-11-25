// ==========================================================
// 2단 스파이럴 부스터
//   각도: 3시 = 0도, 6시 = 90도, 시계 방향으로 증가 (12시 = 270도)
//   배경 그림(노란 링, 초록/파랑 링, STOP) 위에 코드로 그림:
//   - 등장(B2_SLIDE_MS): 화면이 오른쪽에서 밀고 들어오는 동안은 가득 찬 빨간 고리와 시작 숫자를 보여 줌 (ui_main 이 밀어 줌)
//   - 그 뒤 숫자 5 -> 1, 하나에 B2_STEP_MS
//       빨간 고리는 5등분. 매 숫자가 바뀔 때 12시 왼쪽부터 반시계 방향으로 한 영역씩 지워짐
//       흰 점 24개: 숫자가 바뀔 때마다 전부 꺼졌다가, 12시 왼쪽부터 반시계 방향으로 1초에 한 바퀴 켜짐
// ==========================================================
#define B2_SECT   (360.0f / B2_START_NUM)      // 한 영역의 각도 (72도)
#define B2_FULL   630.0f                       // 270 + 360 : 빨간 고리가 가득 찬 상태의 끝 각도

static inline void b2_pt(lv_point_t &p, const lv_area_t &ar, float r, float deg) {
  float a = deg * 3.1415926f / 180.0f;
  p.x = ar.x1 + 233 + (lv_coord_t)lroundf(r * cosf(a));
  p.y = ar.y1 + 233 + (lv_coord_t)lroundf(r * sinf(a));
}

// i 번째 점의 각도를 270 ~ 630 범위로 (빨간 영역 [270, b2_edge] 안에 있는지 볼 때 사용)
static inline float b2_dot_ang(int i) {
  float a = 270.0f - 7.5f - i * (360.0f / B2_DOTS);
  while (a < 270.0f) a += 360.0f;
  return a;
}

static void b2_layer_draw_cb(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_target(e);
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  lv_area_t ar;
  lv_obj_get_coords(o, &ar);

  // 1. 빨간 고리 (270도 -> b2_edge, 시계 방향. 4도 조각으로 나눠 채움)
  lv_draw_rect_dsc_t fill;
  fill_dsc_init(&fill, COL_RED);
  const float s0 = 270.0f;
  const float s1 = b2_edge;
  for (float a = s0; a < s1 - 0.01f; a += 4.0f) {
    float a1 = a + 4.0f;
    float a2 = a1 + 0.6f;                  // 조각 사이 이음새가 안 보이게 살짝 겹침
    if (a1 > s1) { a1 = s1; a2 = s1; }
    lv_point_t p[4];
    b2_pt(p[0], ar, B2_RED_R0, a);
    b2_pt(p[1], ar, B2_RED_R1, a);
    b2_pt(p[2], ar, B2_RED_R1, a2);
    b2_pt(p[3], ar, B2_RED_R0, a2);
    lv_draw_polygon(ctx, &fill, p, 4);
  }

  // 2. 영역 구분선 (빨간 영역 안에 있는 것만)
  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = lv_color_hex(COL_DIVIDER);
  ln.width = 2;
  ln.opa = LV_OPA_COVER;
  for (int k = 0; k < B2_START_NUM; k++) {
    float a = (k == 0) ? 270.0f : (B2_FULL - B2_SECT * k);
    bool in = (k == 0) ? (s1 > 272.0f) : (a <= s1 + 0.01f);
    if (!in) continue;
    lv_point_t q0, q1;
    b2_pt(q0, ar, B2_RED_R0, a);
    b2_pt(q1, ar, B2_RED_R1, a);
    lv_draw_line(ctx, &ln, &q0, &q1);
  }

  // 3. 화살표 4개 + 작은 네모 2개 (빨간 고리 위에)
  if (b2_deco) {
    lv_draw_rect_dsc_t af;
    fill_dsc_init(&af, COL_ARROW);
    lv_draw_line_dsc_t al;
    lv_draw_line_dsc_init(&al);
    al.color = lv_color_hex(COL_ARROW_E);
    al.width = 1;
    al.opa = LV_OPA_COVER;
    for (int k = 0; k < 4; k++) {
      draw_poly(ctx, &af, &B2_ARROW[k][0][0], 3, ar.x1, ar.y1);
      lv_point_t t[3];
      for (int j = 0; j < 3; j++) { t[j].x = ar.x1 + B2_ARROW[k][j][0]; t[j].y = ar.y1 + B2_ARROW[k][j][1]; }
      for (int j = 0; j < 3; j++) lv_draw_line(ctx, &al, &t[j], &t[(j + 1) % 3]);
    }
    for (int k = 0; k < 2; k++) draw_poly(ctx, &af, &B2_SQUARE[k][0][0], 4, ar.x1, ar.y1);
  }

  // 4. 점 24개: 켜진 것 = 흰색, 빨간 영역 안의 꺼진 것 = 검정 (검은 영역의 꺼진 점은 안 그림)
  lv_draw_rect_dsc_t dd;
  lv_draw_rect_dsc_init(&dd);
  dd.bg_opa = LV_OPA_COVER;
  dd.radius = LV_RADIUS_CIRCLE;
  dd.border_width = 0;
  for (int i = 0; i < B2_DOTS; i++) {
    bool lit = (i < b2_lit);
    if (!lit && b2_dot_ang(i) > s1) continue;
    lv_area_t da;
    da.x1 = ar.x1 + B2_DOT[i][0] - B2_DOT_D / 2;
    da.y1 = ar.y1 + B2_DOT[i][1] - B2_DOT_D / 2;
    da.x2 = da.x1 + B2_DOT_D - 1;
    da.y2 = da.y1 + B2_DOT_D - 1;
    dd.bg_color = lit ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
    lv_draw_rect(ctx, &dd, &da);
  }
}

// i 번째 점 부분만 다시 그리기
static void b2_inval_dot(int i) {
  lv_area_t ar, a;
  lv_obj_get_coords(b2_layer, &ar);
  a.x1 = ar.x1 + B2_DOT[i][0] - B2_DOT_D / 2 - 1;
  a.y1 = ar.y1 + B2_DOT[i][1] - B2_DOT_D / 2 - 1;
  a.x2 = ar.x1 + B2_DOT[i][0] + B2_DOT_D / 2 + 1;
  a.y2 = ar.y1 + B2_DOT[i][1] + B2_DOT_D / 2 + 1;
  lv_obj_invalidate_area(b2_layer, &a);
}

static void create_boost2_ui(lv_obj_t *scr) {
  boost2_cont = mk_cont(scr);

  if (b2_bg_ok) {
    lv_obj_t *bg = lv_img_create(boost2_cont);
    lv_img_set_src(bg, &b2_bg_dsc);
    lv_obj_set_pos(bg, 0, 0);
    no_scroll(bg);
  }

  b2_layer = mk_layer(boost2_cont, 0, 0, 466, 466);
  lv_obj_add_event_cb(b2_layer, b2_layer_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

  segnum_create(b2_num, boost2_cont, 233, 233, &SEG_B2_O[0][0][0], NULL, SEG_B2_W, SEG_B2_H, SEG_B2_ADV, 1);
  segnum_force(b2_num, B2_START_NUM);
}

static void b2_start() {
  b2_start_ms = phase_ms;
  b2_edge = B2_FULL;               // 빨간 고리가 가득 찬 상태로 등장
  b2_lit = 0;
  b2_deco = true;
  segnum_force(b2_num, B2_START_NUM);
  lv_obj_invalidate(b2_layer);
}

static void b2_update(uint32_t now) {
  uint32_t el = now - b2_start_ms;
  if ((int32_t)el < 0) el = 0;

  float e;
  int lit, num;
  bool deco;
  bool settled = false;                              // 빨간 고리가 움직이지 않는 상태
  if (el < (uint32_t)B2_SLIDE_MS) {                  // 밀고 들어오는 중: 아직 시작하지 않은 모습 그대로
    e = B2_FULL;
    lit = 0;
    deco = true;
    num = B2_START_NUM;
    settled = true;
  } else {
    uint32_t t = el - (uint32_t)B2_SLIDE_MS;
    int s = (int)(t / (uint32_t)B2_STEP_MS);         // 지워진 영역 수
    if (s > B2_START_NUM - 1) s = B2_START_NUM - 1;
    uint32_t tm = t - (uint32_t)s * B2_STEP_MS;      // 이번 숫자 안에서 지난 시간

    e = B2_FULL - B2_SECT * s;
    settled = true;
    if (s > 0 && tm < (uint32_t)B2_ERASE_MS) {
      settled = false;       // 숫자가 바뀐 직후: 한 영역을 반시계 방향으로 쓸어냄
      e = B2_FULL - B2_SECT * (s - 1) - B2_SECT * (float)tm / (float)B2_ERASE_MS;
    }

    // 흰 점: 숫자 하나(B2_STEP_MS) 동안 한 바퀴. 끝 무렵에 24개가 모두 켜지고, 다음 숫자에서 다시 0 부터
    lit = (int)((tm * (uint32_t)(B2_DOTS + 1)) / (uint32_t)B2_STEP_MS);
    if (lit > B2_DOTS) lit = B2_DOTS;
    deco = true;
    num = B2_START_NUM - s;
  }

  bool full = false;
  if (fabsf(e - b2_edge) >= 0.5f || (settled && e != b2_edge) || deco != b2_deco) {
    b2_edge = e;
    b2_deco = deco;
    full = true;
  }
  if (lit != b2_lit) {
    if (lit < b2_lit) full = true;                   // 새 숫자: 점이 전부 꺼짐
    else for (int i = b2_lit; i < lit; i++) b2_inval_dot(i);
    b2_lit = lit;
  }
  if (full) lv_obj_invalidate(b2_layer);

  if (num != b2_num.val) {
    if (el < (uint32_t)B2_SLIDE_MS) segnum_force(b2_num, num);
    else segnum_set(b2_num, num, now);
  }
  segnum_tick(b2_num, now);
}
