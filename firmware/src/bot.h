#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "face.h"
#include "screens.h"
#include "sound.h"

// Everything Goli does, without any hardware: gestures, moods, alerts, focus, demo, pages, sleep.
// The firmware (main.cpp) feeds it what the sensors report and the browser simulator (sim/) feeds
// it the mouse; both get the same Goli. Sounds go out through sound::play().
namespace bot {

struct Hooks {
  void (*brightness)(int level) = nullptr;           // 0..255
  int (*rotation)() = nullptr;                       // the screen's quarter turns, for swipe directions
  void (*rotate)() = nullptr;                        // turn the screen by 90° (and remember it)
  void (*setup)() = nullptr;                         // 5 taps: open setup mode
  const char *(*hint)() = nullptr;                   // hint under the mouth ("no Wi-Fi..."), nullptr = none
  void (*overview)(Screens &, uint32_t now) = nullptr;  // fill the overview page
  void (*status)(char *out, size_t cap) = nullptr;   // the status page's text
  Sound (*alertSound)(AlertLevel level) = nullptr;   // what an alert sounds like (nullptr: bell, bell twice, chirp)
  bool (*talks)() = nullptr;                         // Goli's own voice for boops and cheers
  bool (*buddy)() = nullptr;                         // desk buddy: good morning, stretch breaks, Friday
  int (*breakMin)() = nullptr;                       // minutes at the desk before a stretch break
  const char *(*birthday)() = nullptr;               // "MM-DD", or nullptr
  void (*alerted)(AlertLevel level) = nullptr;       // an alert rang here (tell the team)
  void (*dayOver)(int alerts, int critical) = nullptr;  // at midnight: remember today for tomorrow's mood
  bool (*quietHours)() = nullptr;                    // night: no sounds from the desk buddy either
  int (*focusMin)() = nullptr;                       // how long focus mode lasts (nullptr: FOCUS_MIN)
};

struct Meeting {
  char title[48];
  int64_t start;  // Unix seconds
};

void begin(lv_obj_t *screen, const Hooks &hooks, uint32_t now);
void update(uint32_t now);  // every loop

void touch(int x, int y, uint32_t now);  // a touch sample while a finger is on the glass (panel pixels)
void lift(uint32_t now);  // optional: the finger left the glass (else noticed 120 ms after the last sample)
void motion(float ax, float ay, float az, uint32_t now);  // accelerometer, in g
// prod_deploy: something went to production (Goli frowns at that on a Friday afternoon)
void alert(const char *title, int count, AlertLevel level, bool quiet_hours, uint32_t now, bool prod_deploy = false);
void clap(uint32_t now);
void loud(bool on);
void power(int percent, bool charging, bool on_battery);  // percent < 0 = no battery

void meetings(const Meeting *m, int n);   // today's meetings from now on, earliest first
void weather(uint8_t accessories);        // what the weather says to wear (Accessory bits)
void presence(uint32_t now);              // someone is at the desk (voices, typing, moving him)
void knock(uint32_t now);                 // two knocks on the desk: snooze alerts for 10 minutes
void teamAlert(int dir, uint32_t now);    // a Goli next to him rang: look that way (-1 left, +1 right)
void yesterday(int alerts, int critical); // how rough yesterday was (how tired he wakes up)
void pages(bool goliash);                 // which swipe pages make sense
void say(const char *text, uint32_t ms, uint32_t now);  // a short message under his mouth

// From the web page
void poke(uint32_t now);            // like a tap
void focus(uint32_t now);           // focus mode on / off
void demo(uint32_t now);            // demo loop on / off
void pairCode(int code, uint32_t now);  // show a pairing code (0 hides it)

}  // namespace bot
