#pragma once

#include <lvgl.h>

// The screen Goli shows in setup mode: a QR code to join his hotspot, then one to open the page.
class SetupUi {
 public:
  void begin(lv_obj_t *screen);
  void showJoin(const char *ssid, const char *pass);  // step 1: join the hotspot
  void showOpen(const char *url, const char *title = "Now open");  // step 2: open the page
  void showSaved();                                  // done, restarting

 private:
  void setQr(const char *text);
  lv_obj_t *title_ = nullptr, *qr_ = nullptr, *line1_ = nullptr, *line2_ = nullptr;
};
