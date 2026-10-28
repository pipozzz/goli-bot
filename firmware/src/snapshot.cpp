// Host-only (env:snapshot): renders each mood into snapshots/<mood>.ppm.
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "face.h"
#include "selftest_ui.h"
#include "setup_ui.h"

uint32_t g_fake_millis = 0;

namespace {

uint16_t fb[LCD_SIZE * LCD_SIZE];
lv_color_t buf[LCD_SIZE * 60];

void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  for (int y = a->y1; y <= a->y2; y++)
    for (int x = a->x1; x <= a->x2; x++) fb[y * LCD_SIZE + x] = (px++)->full;
  lv_disp_flush_ready(drv);
}

void save(const char *name) {
  char path[128];
  snprintf(path, sizeof path, "snapshots/%s.ppm", name);
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6 %d %d 255\n", LCD_SIZE, LCD_SIZE);
  for (uint16_t c : fb) {
    const uint8_t rgb[3] = {uint8_t((c >> 11) << 3), uint8_t(((c >> 5) & 63) << 2), uint8_t((c & 31) << 3)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

// Advance fake time, letting the face animate and LVGL redraw.
void run(Face &face, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) {
    g_fake_millis += 10;
    lv_tick_inc(10);
    face.update(g_fake_millis);
    lv_timer_handler();
  }
}

}  // namespace

int main() {
  mkdir("snapshots", 0755);
  lv_init();
  static lv_disp_draw_buf_t db;
  lv_disp_draw_buf_init(&db, buf, nullptr, LCD_SIZE * 60);
  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = drv.ver_res = LCD_SIZE;
  drv.flush_cb = flush;
  drv.draw_buf = &db;
  lv_disp_drv_register(&drv);

  Face face;
  face.begin(lv_scr_act());

  struct { Mood m; const char *name; bool pet; } shots[] = {
      {Mood::TIRED, "01_tired", false},      {Mood::HAPPY, "02_happy", false},     {Mood::SURPRISED, "03_surprised", false},
      {Mood::DIZZY, "04_dizzy", false},      {Mood::WINK, "05_wink", false},       {Mood::TIRED, "06_petting", true},
      {Mood::DROWSY, "07_drowsy", false},    {Mood::ASLEEP, "08_asleep", false},   {Mood::SURPRISED, "09_goliash", false},
      {Mood::ALARMED, "10_critical", false}, {Mood::HAPPY, "11_good news", false}, {Mood::TIRED, "12_low battery", false},
  };
  for (auto &s : shots) {
    if (!strcmp(s.name, "09_goliash")) face.showAlert("staging: payments-api 1.4.2 deployed", 1, AlertLevel::INFO);
    if (!strcmp(s.name, "10_critical")) face.showAlert("prod: payments-api drift detected", 3, AlertLevel::CRITICAL);
    if (!strcmp(s.name, "11_good news")) {
      face.clearAlert();
      face.showAlert("agent eu-west-1 is back", 1, AlertLevel::GOOD);
    }
    if (!strcmp(s.name, "12_low battery")) {
      face.clearAlert();
      face.showBattery(12, false);
    }
    face.setPetting(s.pet);
    face.setMood(s.m);
    run(face, 1200);
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    save(s.name);
    printf("saved %s\n", s.name);
  }
  // Self-test screen
  SelfTestUi test;
  test.begin(lv_scr_act(), "v0.1.0");
  test.setLine(0, "touch: 214, 288", true);
  test.setLine(1, "tilt: +0.12  -0.03", true);
  test.setLine(2, "sound: ding #2", true);
  test.setLine(3, "Wi-Fi: -58 dBm", true);
  test.setLine(4, "power: no PMU", false);
  test.setTilt(0.12f, -0.03f);
  test.setTouch(true, 214, 288);
  lv_refr_now(nullptr);
  save("selftest");

  // The setup screen replaces the face (its objects are gone, so no more face.update())
  SetupUi setup;
  setup.begin(lv_scr_act());
  setup.showJoin("Goli-1A2B", "48201937");
  lv_refr_now(nullptr);
  save("setup_1_join");
  setup.showOpen("http://192.168.4.1");
  lv_refr_now(nullptr);
  save("setup_2_open");
  return 0;
}
