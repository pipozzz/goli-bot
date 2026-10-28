#include "setup_ui.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

namespace {

const uint32_t kLight = 0x8e99f0;
const int kQr = 200;

lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int y) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_width(l, 330);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
  return l;
}

}  // namespace

void SetupUi::begin(lv_obj_t *screen) {
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  title_ = text(screen, &lv_font_montserrat_28, kLight, 52);
  // White quiet zone around the code so phones read it easily on the black screen
  lv_obj_t *pad = lv_obj_create(screen);
  lv_obj_remove_style_all(pad);
  lv_obj_set_size(pad, kQr + 24, kQr + 24);
  lv_obj_set_style_bg_color(pad, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(pad, 14, 0);
  lv_obj_align(pad, LV_ALIGN_TOP_MID, 0, 96);
  qr_ = lv_qrcode_create(pad, kQr, lv_color_black(), lv_color_white());
  lv_obj_center(qr_);
  line1_ = text(screen, &lv_font_montserrat_28, 0xffffff, 334);
  line2_ = text(screen, &lv_font_montserrat_28, kLight, 372);
}

void SetupUi::setQr(const char *s) { lv_qrcode_update(qr_, s, strlen(s)); }

void SetupUi::showJoin(const char *ssid, const char *pass) {
  char wifi[96];
  snprintf(wifi, sizeof wifi, "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
  setQr(wifi);
  lv_label_set_text(title_, "Set me up!");
  lv_label_set_text(line1_, ssid);
  lv_label_set_text_fmt(line2_, "password %s", pass);
}

void SetupUi::showOpen(const char *url, const char *title) {
  setQr(url);
  lv_label_set_text(title_, title);
  lv_label_set_text(line1_, url + (strncmp(url, "http://", 7) ? 0 : 7));
  lv_label_set_text(line2_, "(or wait for the popup)");
}

void SetupUi::showSaved() {
  lv_obj_add_flag(lv_obj_get_parent(qr_), LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(title_, "");
  lv_obj_align(line1_, LV_ALIGN_CENTER, 0, -20);
  lv_obj_align(line2_, LV_ALIGN_CENTER, 0, 24);
  lv_label_set_text(line1_, "Thanks!");
  lv_label_set_text(line2_, "restarting...");
}
