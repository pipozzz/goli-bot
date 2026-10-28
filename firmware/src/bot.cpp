#include "bot.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>

#include "config.h"
#include "demo.h"
#include "motion.h"
#include "sound.h"

namespace bot {
namespace {

Hooks hk;
Face face;
Screens screens;
Motion mo;

// ---- touch gesture tracking
struct {
  bool down = false;
  uint32_t start = 0, last = 0;
  int16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0, travel = 0;
  bool petting = false, rotated = false;
} tp;

// Quick taps in a row: 1 = boop, 2 = wink, FOCUS_TAPS = focus, SETUP_TAPS = setup, DEMO_TAPS = demo
int taps = 0;
uint32_t last_tap = 0;
uint32_t page_since = 0;   // when the current swipe page was opened or touched
uint32_t focus_until = 0;  // focus mode end (0 = off)

// ---- mood sequencing
uint32_t last_activity = 0;
uint32_t happy_after = 0;  // boop: surprised first, then happy
bool asleep = false;
uint32_t asleep_since = 0;
bool on_battery = false;

void setBrightness(int want) {
  static int current = -1;
  if (want != current && hk.brightness) hk.brightness(current = want);
}
void setBrightness(bool awake) { setBrightness(awake ? BRIGHTNESS_AWAKE : BRIGHTNESS_ASLEEP); }

Mood resting() { return focus_until ? Mood::FOCUSED : Mood::TIRED; }

void wake(uint32_t now) {
  last_activity = now;
  if (asleep) {
    asleep = false;
    setBrightness(true);
    face.setBaseMood(resting());
    face.setMood(Mood::SURPRISED, 700);
  } else if (face.mood() == Mood::DROWSY) {
    face.setBaseMood(resting());
  }
}

void toggleFocus(uint32_t now) {
  if (focus_until) {
    focus_until = 0;
    face.setFocus(-1, 0);
    face.setBaseMood(Mood::TIRED);
    sound::play(Sound::BOOP);
  } else {
    focus_until = now + FOCUS_MIN * 60000UL;
    face.setBaseMood(Mood::FOCUSED);
    face.setFocus(FOCUS_MIN * 60, FOCUS_MIN * 60);
    sound::play(Sound::CHIRP);
  }
}

// A swipe in panel coordinates, turned into the face's orientation: next or previous page
void swipe(int dx, int dy) {
  for (int r = hk.rotation ? hk.rotation() : 0; r > 0; r--) {
    const int t = dx;  // same mapping as LVGL's input rotation
    dx = -dy;
    dy = t;
  }
  const int along = abs(dx) >= abs(dy) ? dx : dy;
  screens.show(screens.page() + (along < 0 ? 1 : -1));
}

void releaseTouch(uint32_t now) {
  const uint32_t held = now - tp.start;
  tp.down = false;
  face.setPetting(false);
  const int dx = tp.x1 - tp.x0, dy = tp.y1 - tp.y0;
  if (!tp.petting && !tp.rotated && held < 350 && tp.travel < 30) {
    taps = now - last_tap < 500 ? taps + 1 : 1;
    last_tap = now;
    return;
  }
  taps = 0;  // a swipe, a hold or petting is not part of a tap sequence
  if (!tp.petting && !tp.rotated && held < 600 && std::max(abs(dx), abs(dy)) > 100) {
    if (demo::active()) demo::stop(face, resting());
    swipe(dx, dy);
  }
}

void updateTouch(uint32_t now) {
  if (tp.down) {
    const uint32_t held = now - tp.start;
    if (now - tp.last > 120) {  // finger lifted
      releaseTouch(now);
      return;
    }
    if (!tp.rotated && tp.travel < 25 && held > ROTATE_HOLD_MS) {
      tp.rotated = true;
      face.setPetting(false);
      if (hk.rotate) hk.rotate();
      face.setMood(Mood::SURPRISED, 800);
    } else if (!tp.rotated && held > 500 && screens.page() == Screens::FACE) {
      tp.petting = true;
      face.setPetting(true);
    }
  }
  if (!taps || tp.down || now - last_tap <= 500) return;

  // The tap sequence is over
  const int n = taps;
  taps = 0;
  if (demo::active()) {  // any tap ends the demo
    demo::stop(face, resting());
    return;
  }
  if (screens.page() != Screens::FACE) {  // a tap on a page goes back to the face
    screens.show(Screens::FACE);
    return;
  }
  if (n >= DEMO_TAPS) {
    demo::start(face);
    return;
  }
  if (n >= SETUP_TAPS) {
    if (hk.setup) hk.setup();
    return;
  }
  if (n == FOCUS_TAPS) {
    toggleFocus(now);
    return;
  }
  if (n == 2) {
    face.setMood(Mood::WINK, 1300);
    sound::play(Sound::CHIRP);
  } else {  // boop!
    face.setMood(Mood::SURPRISED, 450);
    sound::play(Sound::BOOP);
    happy_after = now + 450;
  }
  face.clearAlert();  // any tap means "seen"
}

// Once a second: focus countdown, hints, the night clock and the swipe pages
void tickSecond(uint32_t now) {
  static uint32_t next = 0;
  if (int32_t(now - next) < 0) return;
  next = now + 1000;

  if (focus_until) {
    const int32_t left = int32_t(focus_until - now) / 1000;
    if (left <= 0) {  // done!
      focus_until = 0;
      face.setFocus(-1, 0);
      face.setBaseMood(Mood::TIRED);
      wake(now);
      face.setMood(Mood::HAPPY, 3000);
      sound::play(Sound::CHIRP);
    } else {
      face.setFocus(left, FOCUS_MIN * 60);
    }
  }

  if (!demo::active()) face.showStatus(hk.hint ? hk.hint() : nullptr);  // the demo shows its own hint

  const time_t t = time(nullptr);
  const bool clock_ok = t > 1700000000;
  struct tm lt;
  localtime_r(&t, &lt);
  char hhmm[8], date[32];
  strftime(hhmm, sizeof hhmm, "%H:%M", &lt);
  strftime(date, sizeof date, "%a %d %b", &lt);
  face.setClock(clock_ok ? hhmm : nullptr);

  switch (screens.page()) {
    case Screens::OVERVIEW:
      if (hk.overview) hk.overview(screens, now);
      break;
    case Screens::CLOCK:
      screens.setClock(clock_ok ? hhmm : "--:--", clock_ok ? date : "no time yet");
      break;
    case Screens::STATUS: {
      char text[256] = "";
      if (hk.status) hk.status(text, sizeof text);
      screens.setStatus(text);
      break;
    }
    default:
      break;
  }
  if (screens.page() != Screens::FACE && now - page_since > PAGE_TIMEOUT_MS) screens.show(Screens::FACE);
}

void updateIdle(uint32_t now) {
  if (happy_after && int32_t(now - happy_after) >= 0) {
    happy_after = 0;
    face.setMood(Mood::HAPPY, 2200);
    sound::play(Sound::CHIRP);
  }
  static uint32_t flat_since = 0;
  flat_since = mo.lyingFlat() ? (flat_since ? flat_since : now) : 0;
  const uint32_t idle = now - std::max(last_activity, mo.lastMoveMs());
  if (focus_until || demo::active()) return;  // no napping on the job

  if (asleep && on_battery && now - asleep_since > SCREEN_OFF_AFTER_MS) {
    setBrightness(0);  // on battery and nobody around: screen off until touched or moved
  }
  if (!asleep && (idle > ASLEEP_AFTER_MS || (flat_since && now - flat_since > 3000))) {
    asleep = true;
    asleep_since = now;
    face.setMood(Mood::ASLEEP);
    sound::play(Sound::YAWN);
    setBrightness(false);
  } else if (!asleep && idle > DROWSY_AFTER_MS && face.mood() == Mood::TIRED) {
    face.setBaseMood(Mood::DROWSY);
  } else if (asleep && !flat_since && idle < 500) {
    wake(now);
  }
}

}  // namespace

void begin(lv_obj_t *screen, const Hooks &hooks, uint32_t now) {
  hk = hooks;
  face.begin(screen);
  screens.begin(screen);  // on top of the face
  last_activity = now;
  setBrightness(true);
}

void update(uint32_t now) {
  updateTouch(now);
  demo::update(now);
  tickSecond(now);
  updateIdle(now);
  face.update(now);
}

void touch(int x, int y, uint32_t now) {
  if (!tp.down) {
    tp = {};
    tp.down = true;
    tp.start = now;
    tp.x0 = x;
    tp.y0 = y;
  }
  tp.travel = std::max<int16_t>(tp.travel, abs(x - tp.x0) + abs(y - tp.y0));
  tp.x1 = x;
  tp.y1 = y;
  tp.last = now;
  page_since = now;
  wake(now);
}

void lift(uint32_t now) {
  if (tp.down) releaseTouch(now);
}

void motion(float ax, float ay, float az, uint32_t now) {
  mo.feed(ax, ay, az, now);
  face.setTilt(mo.tiltX(), mo.tiltY());
  if (mo.shaking()) {
    wake(now);
    if (face.mood() != Mood::DIZZY) sound::play(Sound::WOBBLE);
    face.setMood(Mood::DIZZY, 2500);
  } else if (mo.takeJolt()) {
    wake(now);
    if (face.mood() != Mood::DIZZY) face.setMood(Mood::SURPRISED, 600);
  }
}

void alert(const char *title, int count, AlertLevel level, bool quiet_hours, uint32_t now) {
  if (demo::active()) return;
  face.showAlert(title, face.unseen() + count, level);
  if (quiet_hours) return;  // at night: badge only, let him (and you) sleep
  if (focus_until && level != AlertLevel::CRITICAL) return;  // focusing: only what's on fire
  screens.show(Screens::FACE);
  wake(now);
  switch (level) {
    case AlertLevel::CRITICAL:  // an agent went quiet, a vulnerability, prod drift...
      face.setMood(Mood::ALARMED, 4000);
      sound::play(Sound::ALARM);
      break;
    case AlertLevel::GOOD:  // an agent is back, a drift was resolved
      face.setMood(Mood::HAPPY, 2500);
      sound::play(Sound::CHIRP);
      break;
    default:
      face.setMood(Mood::SURPRISED, 1200);  // "furt dačo!"
      sound::play(Sound::DING);
  }
}

void clap(uint32_t now) {
  if (demo::active() || tp.down || now - tp.last < 400) return;  // a knock on the glass is not a clap
  wake(now);
  if (face.unseen()) {  // "got it": alerts seen
    face.clearAlert();
    face.setMood(Mood::WINK, 1000);
  } else {
    face.setMood(Mood::SURPRISED, 600);
  }
}

void loud(bool on) {
  // Wince while it's loud, but never over an alert's face or anything else that is showing
  if (on && !asleep && !demo::active() && (face.mood() == Mood::WINCE || !face.held()))
    face.setMood(Mood::WINCE, 800);
}

void power(int percent, bool charging, bool battery_only) {
  on_battery = battery_only;
  const bool low = battery_only && percent >= 0 && percent < BATTERY_LOW_PCT;
  face.showBattery(charging || low ? percent : -1, charging);
}

}  // namespace bot
