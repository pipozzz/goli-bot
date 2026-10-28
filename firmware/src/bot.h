#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "face.h"
#include "screens.h"

// Everything Goli does, without any hardware: gestures, moods, alerts, focus, demo, pages, sleep.
// The firmware (main.cpp) feeds it what the sensors report and the browser simulator (sim/) feeds
// it the mouse; both get the same Goli. Sounds go out through sound::play().
namespace bot {

struct Hooks {
  void (*brightness)(int level);           // 0..255
  int (*rotation)();                       // the screen's quarter turns, for swipe directions
  void (*rotate)();                        // turn the screen by 90° (and remember it)
  void (*setup)();                         // 5 taps: open setup mode
  const char *(*hint)();                   // hint under the mouth ("no Wi-Fi..."), nullptr = none
  void (*overview)(Screens &, uint32_t now);  // fill the overview page
  void (*status)(char *out, size_t cap);   // the status page's text
};

void begin(lv_obj_t *screen, const Hooks &hooks, uint32_t now);
void update(uint32_t now);  // every loop

void touch(int x, int y, uint32_t now);  // a touch sample while a finger is on the glass (panel pixels)
void lift(uint32_t now);  // optional: the finger left the glass (else noticed 120 ms after the last sample)
void motion(float ax, float ay, float az, uint32_t now);  // accelerometer, in g
void alert(const char *title, int count, AlertLevel level, bool quiet_hours, uint32_t now);
void clap(uint32_t now);
void loud(bool on);
void power(int percent, bool charging, bool on_battery);  // percent < 0 = no battery

}  // namespace bot
