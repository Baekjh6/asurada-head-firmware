// ==========================================================
// 1단 부스터
//   배경 그림(노란 틀, SEC., 작은 빨간 칸) 위에 코드로 그림:
//   - 빨간 꽃잎: 6시 쪽부터 B1_ACTIVE 장(기본 9장)만 켜고, 숫자 하나(B1_STEP_MS)마다 위쪽부터 한 장씩 어두워지며 사라짐
//   - 오른쪽 초록 게이지와 가운데 잎은 숫자 15 이하에서 한 단씩 자람, 9 이하에서 그 위에 빨간 쐐기
//   - 숫자 B1_START_NUM -> B1_END_NUM (기본 14 -> 6)
//   모양(좌표)은 assets.h 의 표 (tools/gen_assets.py 가 만든다)
// ==========================================================
#define B1_ACTIVE  (B1_START_NUM - B1_END_NUM + 1)      // 처음에 켜는 꽃잎 수
#if (B1_START_NUM - B1_END_NUM + 1) > 16 || (B1_START_NUM - B1_END_NUM + 1) < 1
#error "B1_START_NUM - B1_END_NUM + 1 은 1 ~ 16 이어야 합니다 (꽃잎은 16장까지)"
#endif

// 숫자에 따른 게이지 단계 (참고 영상: 초록·잎은 15 부터, 빨간 쐐기는 9 부터)
static int b1_level(int count, int first, int steps) {
  int v = first - count + 1;
  if (v < 0) v = 0;
  if (v > steps) v = steps;
  return v;
}

static void b1_layer_draw_cb(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_target(e);
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  lv_area_t ar;
  lv_obj_get_coords(o, &ar);

  lv_draw_rect_dsc_t red, green, fade;
  fill_dsc_init(&red, COL_RED);
  fill_dsc_init(&green, COL_GREEN);

  if (b1_leaf > 0) draw_poly(ctx, &green, &B1_LEAF[b1_leaf - 1][0][0], B1_LEAF_PTS, ar.x1, ar.y1);

  for (int i = 0; i < b1_cells; i++) draw_poly(ctx, &red, &B1_PETAL[i][0][0], B1_PETAL_PTS, ar.x1, ar.y1);

  if (b1_fade_idx >= 0 && b1_fade_idx < B1_CELLS) {            // 사라지는 중인 꽃잎
    fill_dsc_init(&fade, COL_RED);
    fade.bg_color = lv_color_mix(lv_color_hex(COL_RED), lv_color_hex(0x000000), (uint8_t)b1_fade_k);
    draw_poly(ctx, &fade, &B1_PETAL[b1_fade_idx][0][0], B1_PETAL_PTS, ar.x1, ar.y1);
  }

  if (b1_green > 0) draw_poly(ctx, &green, &B1_GREEN[b1_green - 1][0][0], B1_GREEN_PTS, ar.x1, ar.y1);
  if (b1_red2 > 0)  draw_poly(ctx, &red, &B1_RED2[b1_red2 - 1][0][0], B1_RED2_PTS, ar.x1, ar.y1);
}

static void create_boost1_ui(lv_obj_t *scr) {
  boost1_cont = mk_cont(scr);

  if (b1_bg_ok) {
    lv_obj_t *bg = lv_img_create(boost1_cont);
    lv_img_set_src(bg, &b1_bg_dsc);
    lv_obj_set_pos(bg, 0, 0);
    no_scroll(bg);
  }

  b1_layer = mk_layer(boost1_cont, 0, 0, 466, 466);
  lv_obj_add_event_cb(b1_layer, b1_layer_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

  // 숫자 두 자리 (오른쪽 정렬: 한 자리 숫자는 일의 자리에 표시)
  segnum_create(b1_num, boost1_cont, B1_NUM_CX + SEG_B1_ADV / 2, B1_NUM_CY, &SEG_B1_O[0][0][0], NULL,
                SEG_B1_W, SEG_B1_H, SEG_B1_ADV, 2);
  segnum_force(b1_num, B1_START_NUM);
}

static void b1_start() {
  b1_start_ms = phase_ms;          // seqTask 가 정한 목표 시각
  b1_cells = B1_ACTIVE;
  b1_fade_idx = -1;
  b1_fade_k = 0;
  b1_green = b1_level(B1_START_NUM, 15, B1_GREEN_STEPS);
  b1_red2 = b1_level(B1_START_NUM, 9, B1_RED2_STEPS);
  b1_leaf = b1_level(B1_START_NUM, 15, B1_LEAF_STEPS);
  segnum_force(b1_num, B1_START_NUM);
  lv_obj_invalidate(b1_layer);
}

static void b1_update(uint32_t now) {
  uint32_t el = now - b1_start_ms;
  if ((int32_t)el < 0) el = 0;                       // 시작 시각이 미래일 때(소리 지연 보정)

  int idx = (int)(el / (uint32_t)B1_STEP_MS);        // 지난 칸 수
  if (idx > B1_ACTIVE - 1) idx = B1_ACTIVE - 1;
  uint32_t tm = el - (uint32_t)idx * B1_STEP_MS;     // 이번 칸 안에서 지난 시간
  int count = B1_START_NUM - idx;
  int cells = B1_ACTIVE - idx;

  // 방금 사라진 꽃잎은 B1_FADE_MS 동안 어두워지며 남아 있음
  int fidx = -1, fk = 0;
  if (idx > 0 && tm < (uint32_t)B1_FADE_MS) {
    fidx = cells;
    fk = 255 - (int)((tm * 255U) / (uint32_t)B1_FADE_MS);
  }

  int g  = b1_level(count, 15, B1_GREEN_STEPS);
  int lf = b1_level(count, 15, B1_LEAF_STEPS);
  int r2 = b1_level(count, 9, B1_RED2_STEPS);

  if (cells != b1_cells) {
    if (cells == b1_cells - 1) inval_box(b1_layer, B1_PETAL_BOX[cells]);
    else lv_obj_invalidate(b1_layer);                // 여러 칸을 한꺼번에 건너뜀
    b1_cells = cells;
  }
  if (fidx != b1_fade_idx || fk != b1_fade_k) {
    if (b1_fade_idx >= 0 && b1_fade_idx < B1_CELLS) inval_box(b1_layer, B1_PETAL_BOX[b1_fade_idx]);
    if (fidx >= 0 && fidx < B1_CELLS) inval_box(b1_layer, B1_PETAL_BOX[fidx]);
    b1_fade_idx = fidx;
    b1_fade_k = fk;
  }
  if (g != b1_green || r2 != b1_red2) {
    b1_green = g;
    b1_red2 = r2;
    inval_box(b1_layer, B1_GAUGE_BOX);
  }
  if (lf != b1_leaf) {
    b1_leaf = lf;
    inval_box(b1_layer, B1_LEAF_BOX);
  }

  segnum_set(b1_num, count, now);
  segnum_tick(b1_num, now);
}
