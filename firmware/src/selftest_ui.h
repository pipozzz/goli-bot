#pragma once

#include <lvgl.h>

// Self-test screen for a freshly assembled Goli: what every part reports, live.
class SelfTestUi {
 public:
  void begin(lv_obj_t *screen, const char *version);
  void setTouch(bool down, int x, int y);   // raw panel coordinates
  void setTilt(float roll, float pitch);     // -1..1
  void setLine(int row, const char *text, bool ok);  // rows: 0 touch, 1 tilt, 2 sound, 3 Wi-Fi, 4 battery

 private:
  lv_obj_t *rows_[5] = {}, *dot_ = nullptr, *bubble_ = nullptr;
};
