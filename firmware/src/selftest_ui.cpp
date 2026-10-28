#include "selftest_ui.h"

#include "config.h"

namespace {

const uint32_t kLight = 0x8e99f0;
const uint32_t kOk = 0x3fb27f;
const uint32_t kBad = 0xe5484d;
const int kC = LCD_SIZE / 2;

}  // namespace

void SelfTestUi::begin(lv_obj_t *screen, const char *version) {
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // A ring at the edge of the glass (the visible area) and a bubble level in the middle
  lv_obj_t *ring = lv_obj_create(screen);
  lv_obj_remove_style_all(ring);
  lv_obj_set_size(ring, LCD_SIZE - 8, LCD_SIZE - 8);
  lv_obj_center(ring);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(ring, lv_color_hex(0x3b4070), 0);
  lv_obj_set_style_border_width(ring, 4, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(kLight), 0);
  lv_label_set_text_fmt(title, "Self-test  %s", version);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 58);

  for (int i = 0; i < 6; i++) {
    rows_[i] = lv_label_create(screen);
    lv_obj_set_style_text_font(rows_[i], &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(rows_[i], lv_color_white(), 0);
    lv_obj_set_width(rows_[i], 340);
    lv_obj_set_style_text_align(rows_[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(rows_[i], LV_ALIGN_TOP_MID, 0, 108 + 40 * i);
    lv_label_set_text(rows_[i], "");
  }
  lv_obj_t *hint = lv_label_create(screen);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(0x9a9fb8), 0);
  lv_label_set_text(hint, "tap = ding    hold 3 s = exit");
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -36);

  bubble_ = lv_obj_create(screen);
  lv_obj_remove_style_all(bubble_);
  lv_obj_set_size(bubble_, 30, 30);
  lv_obj_set_style_radius(bubble_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bubble_, lv_color_hex(kLight), 0);
  lv_obj_set_style_bg_opa(bubble_, LV_OPA_60, 0);

  dot_ = lv_obj_create(screen);
  lv_obj_remove_style_all(dot_);
  lv_obj_set_size(dot_, 56, 56);
  lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(dot_, lv_color_hex(kOk), 0);
  lv_obj_set_style_border_width(dot_, 6, 0);
  lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);

  setTilt(0, 0);
}

void SelfTestUi::setTouch(bool down, int x, int y) {
  if (!down) {
    lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_set_pos(dot_, x - 28, y - 28);
  lv_obj_clear_flag(dot_, LV_OBJ_FLAG_HIDDEN);
}

void SelfTestUi::setTilt(float roll, float pitch) {
  lv_obj_set_pos(bubble_, kC - 15 + int(roll * 140), kC + 150 - 15 + int(pitch * 25));
}

void SelfTestUi::setLine(int row, const char *text, bool ok) {
  if (row < 0 || row >= 6) return;
  lv_label_set_text(rows_[row], text);
  lv_obj_set_style_text_color(rows_[row], lv_color_hex(ok ? 0xffffff : kBad), 0);
}
