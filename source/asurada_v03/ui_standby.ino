// ==========================================================
// 대기화면: 입체 구슬 4개 (2 x 2)
//   - 화면에 들어올 때 돌면서 나타남 (끝으로 갈수록 느려짐)
//   - ORB_TURN_EVERY_MS 마다 90도 회전 (천천히 시작해서 천천히 멈춤)
//   - 번인 방지: SHIFT_EVERY_MS 마다 전체 위치를 몇 픽셀씩 옮김
// ==========================================================
static void orbs_place(float ang, float rad) {
  for (int i = 0; i < 4; i++) {
    float a = (ang + i * 90.0f) * 3.1415926f / 180.0f;
    int px = (int)lroundf(233.0f + rad * cosf(a)) - ORB_SIZE / 2 + shift_x;
    int py = (int)lroundf(233.0f + rad * sinf(a)) - ORB_SIZE / 2 + shift_y;
    lv_obj_set_pos(orb_img[i], px, py);
  }
}

static void create_standby_ui(lv_obj_t *scr) {
  standby_cont = mk_cont(scr);

  for (int i = 0; i < 4; i++) {
    orb_img[i] = lv_img_create(standby_cont);
    lv_img_set_src(orb_img[i], &orb_dsc);
    no_scroll(orb_img[i]);
  }
  orbs_place(orb_ang, orb_rad);

  // 음성인식 "Hi Asurada" 를 들으면 가운데에 표시
  listen_lbl = lv_label_create(standby_cont);
  lv_label_set_text(listen_lbl, "LISTENING");
  lv_obj_set_style_text_color(listen_lbl, lv_color_hex(COL_CYAN), 0);
  lv_obj_set_style_text_font(listen_lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(listen_lbl, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(listen_lbl, LV_OBJ_FLAG_HIDDEN);
}

// 대기화면으로 들어올 때. spin = 돌면서 나타나기
static void standby_enter(bool spin, uint32_t now) {
  orb_turn_ms = now;
  if (spin) {
    orb_from_ang = 45.0f - 270.0f;       // 3/4 바퀴 돌면서
    orb_to_ang = 45.0f;
    orb_from_rad = 92.0f;                // 안쪽에서 바깥으로 (구슬끼리 겹치지 않는 최소 반지름)
    orb_anim_ms = now;
    orb_anim_len = ORB_SPIN_IN_MS;
    orb_anim_spin = true;
    orb_ang = orb_from_ang;
    orb_rad = orb_from_rad;
  } else {
    orb_anim_len = 0;
    orb_ang = 45.0f;
    orb_rad = ORB_RADIUS;
  }
  orbs_place(orb_ang, orb_rad);
}

static void standby_update(uint32_t now) {
  if (orb_anim_len) {
    float t = (float)(now - orb_anim_ms) / (float)orb_anim_len;
    if (t >= 1.0f) {
      orb_anim_len = 0;
      orb_ang = 45.0f;                   // 끝나면 항상 같은 모양 (4개가 대칭이라 90도 회전 = 같은 자리)
      orb_rad = ORB_RADIUS;
      orb_turn_ms = now;
    } else {
      float k = orb_anim_spin ? ease_out_cubic(t) : ease_in_out_cubic(t);
      orb_ang = orb_from_ang + (orb_to_ang - orb_from_ang) * k;
      orb_rad = orb_from_rad + ((float)ORB_RADIUS - orb_from_rad) * k;
    }
    orbs_place(orb_ang, orb_rad);
  } else if ((now - orb_turn_ms) >= (uint32_t)ORB_TURN_EVERY_MS) {
    orb_from_ang = 45.0f;                // 90도 회전 시작
    orb_to_ang = 135.0f;
    orb_from_rad = ORB_RADIUS;
    orb_anim_ms = now;
    orb_anim_len = ORB_TURN_MS;
    orb_anim_spin = false;
  }

#if SHIFT_EVERY_MS > 0
  if ((now - shift_ms) >= (uint32_t)SHIFT_EVERY_MS && !orb_anim_len) {
    shift_ms = now;
    shift_idx = (shift_idx + 1) % 8;
    float a = shift_idx * 45.0f * 3.1415926f / 180.0f;
    shift_x = (int)lroundf(SHIFT_RADIUS * cosf(a));
    shift_y = (int)lroundf(SHIFT_RADIUS * sinf(a));
    orbs_place(orb_ang, orb_rad);
  }
#endif
}
