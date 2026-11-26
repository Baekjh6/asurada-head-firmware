// ==========================================================
// 화면 공용 도구 (loop Task 에서만 호출)
//   - 펌웨어에 들어 있는 그림(assets.h)을 PSRAM 으로 풀기
//   - 좌표 표로 다각형 그리기
//   - 7세그먼트 숫자 (기울어진 획, 바뀔 때 이전 숫자가 잠깐 겹쳐 보임)
// ==========================================================
static void no_scroll(lv_obj_t *o) {
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

// 화면 하나 = 검은 배경의 466x466 컨테이너
static lv_obj_t *mk_cont(lv_obj_t *p) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, 466, 466);
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  no_scroll(o);
  return o;
}

// 직접 그리는 투명 오브젝트
static lv_obj_t *mk_layer(lv_obj_t *p, int px, int py, int w, int h) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, px, py);
  no_scroll(o);
  return o;
}

static inline lv_color_t c565(uint16_t v) {      // RGB565 -> LVGL 색 (LV_COLOR_16_SWAP 설정과 무관)
  uint8_t r = ((v >> 11) & 0x1F) << 3; r |= r >> 5;
  uint8_t g = ((v >> 5) & 0x3F) << 2;  g |= g >> 6;
  uint8_t b = (v & 0x1F) << 3;         b |= b >> 5;
  return lv_color_make(r, g, b);
}

static void img_fill_dsc(lv_img_dsc_t *dsc, lv_color_t *buf, int w, int h) {
  dsc->header.always_zero = 0;
  dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
  dsc->header.w = w;
  dsc->header.h = h;
  dsc->data_size = (uint32_t)w * h * sizeof(lv_color_t);
  dsc->data = (const uint8_t *)buf;
}

// (개수, 값) 쌍으로 된 RLE 그림을 PSRAM 으로 풀어 LVGL 이미지로 만든다
static bool rle_to_img(const uint16_t *rle, uint32_t len, int w, int h, lv_img_dsc_t *dsc) {
  const size_t n = (size_t)w * h;
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(n * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!buf) {
    Serial.println("그림 PSRAM 할당 실패 (Tools > PSRAM > OPI PSRAM 확인)");
    return false;
  }
  size_t idx = 0;
  for (uint32_t i = 0; i + 1 < len && idx < n; i += 2) {
    uint32_t cnt = rle[i];
    lv_color_t c = c565(rle[i + 1]);
    while (cnt-- && idx < n) buf[idx++] = c;
  }
  if (idx < n) {
    Serial.println("그림 자료 길이 오류 (assets.h 를 다시 만드세요)");
    heap_caps_free(buf);
    return false;
  }
  img_fill_dsc(dsc, buf, w, h);
  return true;
}

static bool raw_to_img(const uint16_t *pix, int w, int h, lv_img_dsc_t *dsc) {
  const size_t n = (size_t)w * h;
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(n * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!buf) return false;
  for (size_t i = 0; i < n; i++) buf[i] = c565(pix[i]);
  img_fill_dsc(dsc, buf, w, h);
  return true;
}

void load_images() {
  b1_bg_ok = rle_to_img(B1_BG_RLE, B1_BG_RLE_LEN, ASSET_W, ASSET_W, &b1_bg_dsc);
  b2_bg_ok = rle_to_img(B2_BG_RLE, B2_BG_RLE_LEN, ASSET_W, ASSET_W, &b2_bg_dsc);
  bool orb_ok = raw_to_img(ORB_PIX, ORB_SIZE, ORB_SIZE, &orb_dsc);
  Serial.printf("그림 풀기: 1단 %s, 2단 %s, 구슬 %s\n", b1_bg_ok ? "OK" : "실패", b2_bg_ok ? "OK" : "실패", orb_ok ? "OK" : "실패");
}

// 좌표 표(x0,y0,x1,y1,...)로 다각형을 그린다. (ox, oy) = 더할 위치
static void draw_poly(lv_draw_ctx_t *ctx, const lv_draw_rect_dsc_t *dsc, const int16_t *tbl, int n, int ox, int oy) {
  lv_point_t p[12];
  if (n > 12) n = 12;
  for (int i = 0; i < n; i++) {
    p[i].x = (lv_coord_t)(ox + tbl[2 * i]);
    p[i].y = (lv_coord_t)(oy + tbl[2 * i + 1]);
  }
  lv_draw_polygon(ctx, dsc, p, n);
}

// 오브젝트 안의 네모 영역(좌표 표: x1,y1,x2,y2)만 다시 그리게 함
static void inval_box(lv_obj_t *o, const int16_t *b) {
  lv_area_t ar, a;
  lv_obj_get_coords(o, &ar);
  a.x1 = ar.x1 + b[0];
  a.y1 = ar.y1 + b[1];
  a.x2 = ar.x1 + b[2];
  a.y2 = ar.y1 + b[3];
  lv_obj_invalidate_area(o, &a);
}

static void fill_dsc_init(lv_draw_rect_dsc_t *d, uint32_t color) {
  lv_draw_rect_dsc_init(d);
  d->bg_color = lv_color_hex(color);
  d->bg_opa = LV_OPA_COVER;
  d->radius = 0;
  d->border_width = 0;
}

// ----------------------------------------------------------
// 7세그먼트 숫자
// ----------------------------------------------------------
static void segnum_draw_cb(lv_event_t *e) {
  SegNum *s = (SegNum *)lv_event_get_user_data(e);
  lv_obj_t *o = lv_event_get_target(e);
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  lv_area_t ar;
  lv_obj_get_coords(o, &ar);

  lv_draw_rect_dsc_t white, ghost, black;
  fill_dsc_init(&white, 0xFFFFFF);
  fill_dsc_init(&ghost, COL_GHOST);
  fill_dsc_init(&black, 0x000000);

  int place = 1;                                   // 1, 10, 100
  for (int i = 0; i < s->ndig; i++, place *= 10) {
    bool on  = (i == 0) || (s->val >= place);                      // 이 자리를 그리는가 (앞자리 0 은 숨김)
    bool pon = s->ghost && ((i == 0) || (s->prev >= place));
    uint8_t m  = on  ? DIGIT_MASK[(s->val / place) % 10] : 0;
    uint8_t pm = pon ? DIGIT_MASK[(s->prev / place) % 10] : 0;
    if (!m && !pm) continue;
    int ox = ar.x1 + 2 + s->dw / 2 + (s->ndig - 1 - i) * s->adv;
    int oy = ar.y1 + 2 + s->dh / 2;
    for (int k = 0; k < 7; k++) {
      uint8_t bit = (uint8_t)(1 << k);
      if (!(m & bit) && !(pm & bit)) continue;
      draw_poly(ctx, (m & bit) ? &white : &ghost, s->outer + k * 12, 6, ox, oy);
      if (s->inner) draw_poly(ctx, &black, s->inner + k * 12, 6, ox, oy);
    }
  }
}

// cx_right = 가장 오른쪽 자리의 가운데 x, cy = 가운데 y (부모 기준 좌표)
static void segnum_create(SegNum &s, lv_obj_t *parent, int cx_right, int cy, const int16_t *outer, const int16_t *inner,
                          int dw, int dh, int adv, int ndig) {
  s.outer = outer;
  s.inner = inner;
  s.dw = dw; s.dh = dh; s.adv = adv;
  s.ndig = ndig;
  s.val = 0; s.prev = 0; s.chg_ms = 0; s.ghost = false;
  int w = (ndig - 1) * adv + dw + 4;
  int h = dh + 4;
  s.obj = mk_layer(parent, cx_right - (ndig - 1) * adv - dw / 2 - 2, cy - dh / 2 - 2, w, h);
  lv_obj_add_event_cb(s.obj, segnum_draw_cb, LV_EVENT_DRAW_MAIN, &s);
}

static void segnum_set(SegNum &s, int v, uint32_t now) {       // 값이 바뀌면 이전 숫자를 잠깐 겹쳐 그림
  if (v == s.val) return;
  s.prev = s.val;
  s.val = v;
  s.chg_ms = now;
  s.ghost = (DIGIT_GHOST_MS > 0);
  lv_obj_invalidate(s.obj);
}

static void segnum_force(SegNum &s, int v) {                   // 겹침 없이 바로 표시 (화면이 시작될 때)
  s.val = v;
  s.prev = v;
  s.ghost = false;
  lv_obj_invalidate(s.obj);
}

static void segnum_tick(SegNum &s, uint32_t now) {
  if (s.ghost && (now - s.chg_ms) >= (uint32_t)DIGIT_GHOST_MS) {
    s.ghost = false;
    lv_obj_invalidate(s.obj);
  }
}

static inline float ease_out_cubic(float t) {
  t = 1.0f - t;
  return 1.0f - t * t * t;
}

static inline float ease_in_out_cubic(float t) {
  if (t < 0.5f) return 4.0f * t * t * t;
  t = -2.0f * t + 2.0f;
  return 1.0f - t * t * t * 0.5f;
}
