#include "bot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
int focus_total_s = 0;     // its length, for the ring

// ---- mood sequencing
uint32_t last_activity = 0;
uint32_t happy_after = 0;  // boop: surprised first, then happy
bool asleep = false;
uint32_t asleep_since = 0;
bool on_battery = false;

// ---- the outside world
Meeting meets[6];
int meet_n = 0;
uint8_t weather_mask = 0;
uint32_t snooze_until = 0;      // knocked: alerts stay silent until then
char said[64] = "";             // a message under his mouth, until said_until
uint32_t said_until = 0;

// ---- his day
int today_alerts = 0, today_critical = 0, today_good = 0, today_pets = 0, today_boops = 0;
int yday_alerts = -1, yday_critical = 0;
int day_of_year = -1;

// ---- desk buddy
uint32_t present_last = 0, present_since = 0;
int greeted_day = -1, friday_day = -1;

bool talks() { return hk.talks && hk.talks(); }

Sound alertSound(AlertLevel level) {
  if (hk.alertSound) return hk.alertSound(level);
  return level == AlertLevel::CRITICAL ? Sound::ALARM : level == AlertLevel::GOOD ? Sound::CHIRP : Sound::DING;
}

void daily(const struct tm &lt, uint32_t now);
void updateMeeting(time_t t, uint32_t now);
void wear();

bool snoozed(uint32_t now) { return snooze_until && int32_t(snooze_until - now) > 0; }
uint32_t pairing_until = 0;  // a pairing code is on his face: nothing else takes its line

// The desk buddy speaks up only when sounds are welcome
void chime(Sound s, uint32_t now) {
  if (!snoozed(now) && !(hk.quietHours && hk.quietHours())) sound::play(s);
}

bool localTime(struct tm &lt) {
  const time_t t = time(nullptr);
  localtime_r(&t, &lt);
  return t > 1700000000;
}

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
    const int minutes = hk.focusMin && hk.focusMin() > 0 ? hk.focusMin() : FOCUS_MIN;
    focus_total_s = minutes * 60;
    focus_until = now + focus_total_s * 1000UL;
    face.setBaseMood(Mood::FOCUSED);
    face.setFocus(focus_total_s, focus_total_s);
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
  screens.step(along < 0 ? 1 : -1);
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
    } else if (!tp.rotated && !tp.petting && held > 500 && screens.page() == Screens::FACE) {
      today_pets++;
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
    sound::play(talks() ? Sound::YAY : Sound::CHIRP);
  } else {  // boop!
    today_boops++;
    face.setMood(Mood::SURPRISED, 450);
    sound::play(talks() ? Sound::HM : Sound::BOOP);
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
      face.setFocus(left, focus_total_s);
    }
  }

  if (!demo::active()) {  // the demo shows its own hint
    if (said_until && int32_t(said_until - now) > 0) face.showStatus(said);
    else face.showStatus(hk.hint ? hk.hint() : nullptr);
  }

  const time_t t = time(nullptr);
  const bool clock_ok = t > 1700000000;
  struct tm lt;
  localtime_r(&t, &lt);
  char hhmm[8], date[32];
  strftime(hhmm, sizeof hhmm, "%H:%M", &lt);
  strftime(date, sizeof date, "%a %d %b", &lt);
  face.setClock(clock_ok ? hhmm : nullptr);
  wear();  // a birthday set on the web page shows right away
  if (clock_ok) daily(lt, now);
  updateMeeting(t, now);

  switch (screens.page()) {
    case Screens::OVERVIEW:
      if (hk.overview) hk.overview(screens, now);
      break;
    case Screens::CLOCK:
      screens.setClock(clock_ok ? hhmm : "--:--", clock_ok ? date : "no time yet");
      break;
    case Screens::AGENDA: {
      char text[256] = "";
      size_t used = 0;
      for (int i = 0; i < meet_n && i < 4; i++) {
        const time_t st = time_t(meets[i].start);
        struct tm m;
        localtime_r(&st, &m);
        used += snprintf(text + used, sizeof text - used, "%02d:%02d %.18s\n", m.tm_hour, m.tm_min, meets[i].title);
        if (used >= sizeof text) break;
      }
      screens.setAgenda(meet_n ? text : "no more meetings\ntoday");
      break;
    }
    case Screens::TODAY: {
      char text[256];
      snprintf(text, sizeof text, "%d alerts, %d critical\n%d good news\npetted %dx, booped %dx\n%s",
               today_alerts, today_critical, today_good, today_pets, today_boops,
               yday_alerts < 0 ? "" : (yday_alerts > 15 ? "rough yesterday" : "calm yesterday"));
      screens.setToday(text);
      break;
    }
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

void wear() {
  struct tm lt;
  uint8_t mask = weather_mask;
  if (localTime(lt)) {
    if (lt.tm_mon == 11 && lt.tm_mday <= 26) mask |= SANTA_HAT;
    char today[6];
    snprintf(today, sizeof today, "%02d-%02d", lt.tm_mon + 1, lt.tm_mday);
    const char *bday = hk.birthday ? hk.birthday() : nullptr;
    if (bday && !strcmp(bday, today)) mask |= PARTY_HAT;
  }
  face.setAccessories(mask);
}

// Once a second with a valid clock: a new day, the desk buddy's moments
void daily(const struct tm &lt, uint32_t now) {
  if (lt.tm_yday != day_of_year) {
    if (day_of_year >= 0) {  // midnight: today becomes yesterday
      if (hk.dayOver) hk.dayOver(today_alerts, today_critical);
      yesterday(today_alerts, today_critical);
      today_alerts = today_critical = today_good = today_pets = today_boops = 0;
    }
    day_of_year = lt.tm_yday;
    wear();
  }
  const bool present = present_last && now - present_last < 3 * 60000UL;
  if (!present) present_since = 0;
  if (!hk.buddy || !hk.buddy() || asleep || demo::active() || !present) return;

  if (greeted_day != lt.tm_yday && lt.tm_hour >= 5 && lt.tm_hour < 11) {  // the first hello of the day
    greeted_day = lt.tm_yday;
    const char *bday = hk.birthday ? hk.birthday() : nullptr;
    char today[6];
    snprintf(today, sizeof today, "%02d-%02d", lt.tm_mon + 1, lt.tm_mday);
    face.setMood(Mood::HAPPY, 2500);
    chime(talks() ? Sound::YAY : Sound::CHIRP, now);
    say(bday && !strcmp(bday, today) ? "happy birthday!" : "good morning!", 6000, now);
  }
  if (friday_day != lt.tm_yday && lt.tm_wday == 5 && lt.tm_hour >= 17) {  // weekend!
    friday_day = lt.tm_yday;
    face.setMood(Mood::WINK, 2000);
    chime(talks() ? Sound::YAY : Sound::CHIRP, now);
    say("it's Friday - go home!", 8000, now);
  }
  const int break_min = hk.breakMin ? hk.breakMin() : 50;
  if (break_min > 0 && !focus_until && present_since && now - present_since > break_min * 60000UL) {
    present_since = now;  // the next one in another break_min
    face.setMood(Mood::SURPRISED, 1200);
    chime(talks() ? Sound::HM : Sound::DROP, now);
    say("stretch? water?", 10000, now);
  }
}

// The next meeting: an amber ring for the last MEETING_REMIND_MIN, a ding when it starts
void updateMeeting(time_t t, uint32_t now) {
  while (meet_n && meets[0].start + 60 < int64_t(t)) {  // started over a minute ago: drop it
    for (int i = 1; i < meet_n; i++) meets[i - 1] = meets[i];
    meet_n--;
  }
  static int64_t rang_for = 0;
  if (!meet_n || t < 1700000000) {
    face.setMeeting(nullptr, 0, 0);
    return;
  }
  const int64_t left = meets[0].start - int64_t(t);
  if (left > MEETING_REMIND_MIN * 60) {
    face.setMeeting(nullptr, 0, 0);
    return;
  }
  if (left <= 0) {
    if (rang_for != meets[0].start) {
      rang_for = meets[0].start;
      wake(now);
      face.setMood(Mood::SURPRISED, 1500);
      if (!snoozed(now)) sound::play(alertSound(AlertLevel::INFO));
      say(meets[0].title, 20000, now);
    }
    face.setMeeting(nullptr, 0, 0);
    return;
  }
  if (pairing_until && int32_t(pairing_until - now) > 0) return;  // the code keeps the line
  face.setMeeting(meets[0].title, int(left), MEETING_REMIND_MIN * 60);
}

void updateIdle(uint32_t now) {
  if (happy_after && int32_t(now - happy_after) >= 0) {
    happy_after = 0;
    face.setMood(Mood::HAPPY, 2200);
    sound::play(talks() ? Sound::YAY : Sound::CHIRP);
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
  wear();
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
  presence(now);
  wake(now);
}

void lift(uint32_t now) {
  if (tp.down) releaseTouch(now);
}

void motion(float ax, float ay, float az, uint32_t now) {
  mo.feed(ax, ay, az, now);
  face.setTilt(mo.tiltX(), mo.tiltY());
  if (mo.takeKnock()) knock(now);
  if (mo.shaking()) {
    wake(now);
    if (face.mood() != Mood::DIZZY) sound::play(Sound::WOBBLE);
    face.setMood(Mood::DIZZY, 2500);
  } else if (mo.takeJolt()) {
    wake(now);
    if (face.mood() != Mood::DIZZY) face.setMood(Mood::SURPRISED, 600);
  }
}

void alert(const char *title, int count, AlertLevel level, bool quiet_hours, uint32_t now, bool prod_deploy) {
  if (demo::active()) return;
  today_alerts += count;
  if (level == AlertLevel::CRITICAL) today_critical += count;
  if (level == AlertLevel::GOOD) today_good += count;
  face.showAlert(title, face.unseen() + count, level);
  if (hk.alerted) hk.alerted(level);
  if (quiet_hours || snoozed(now)) return;  // at night or snoozed: badge only
  if (focus_until && level != AlertLevel::CRITICAL) return;  // focusing: only what's on fire
  screens.show(Screens::FACE);
  wake(now);
  struct tm lt;
  if (prod_deploy && localTime(lt) && lt.tm_wday == 5 && lt.tm_hour >= 15) {  // to production, on a Friday?!
    face.setMood(Mood::WINCE, 3000);
    sound::play(talks() ? Sound::EH : alertSound(level));
    say("on a Friday?!", 8000, now);
    return;
  }
  switch (level) {
    case AlertLevel::CRITICAL:  // an agent went quiet, a vulnerability, prod drift...
      face.setMood(Mood::ALARMED, 4000);
      break;
    case AlertLevel::GOOD:  // an agent is back, a drift was resolved
      face.setMood(Mood::HAPPY, 2500);
      break;
    default:
      face.setMood(Mood::SURPRISED, 1200);  // "furt dačo!"
  }
  sound::play(alertSound(level));
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

void meetings(const Meeting *m, int n) {
  meet_n = n < 6 ? n : 6;
  for (int i = 0; i < meet_n; i++) meets[i] = m[i];
}

void weather(uint8_t accessories) {
  weather_mask = accessories & (SUNGLASSES | UMBRELLA | SCARF);
  wear();
}

void presence(uint32_t now) {
  if (!present_last || now - present_last > 3 * 60000UL) present_since = now;  // back at the desk
  present_last = now;
}

void knock(uint32_t now) {
  // Taps on the glass (a wink is two) and his own speaker shake him too: those aren't knocks
  if (demo::active() || tp.down || now - tp.last < 400 || sound::playing()) return;
  snooze_until = now + 10 * 60000UL;
  face.clearAlert();
  face.setMood(Mood::WINK, 1000);
  sound::stop();
  say("snoozed for 10 min", 5000, now);
}

void teamAlert(int dir, uint32_t now) {
  if (asleep || demo::active()) return;
  face.glance(dir, 2500);
  if (!face.held()) face.setMood(Mood::SURPRISED, 900);
  (void)now;
}

void yesterday(int alerts, int critical) {
  yday_alerts = alerts;
  yday_critical = critical;
  const float rough = (alerts + 3.f * critical) / 30.f;  // 30 points = as tired as he gets
  face.setFatigue(rough > 1.f ? 1.f : rough);
}

void pages(bool goliash) { screens.enable(Screens::OVERVIEW, goliash); }

void say(const char *text, uint32_t ms, uint32_t now) {
  snprintf(said, sizeof said, "%s", text);
  said_until = now + ms;
  face.showStatus(said);
}

void poke(uint32_t now) {
  wake(now);
  taps = 1;
  last_tap = now - 600;  // an already finished one-tap sequence: boop
}

void focus(uint32_t now) {
  wake(now);
  toggleFocus(now);
}

void demo(uint32_t now) {
  wake(now);
  if (demo::active()) demo::stop(face, resting());
  else demo::start(face);
}

void pairCode(int code, uint32_t now) {
  if (!code) {
    if (pairing_until) face.showTitle("", 1);
    pairing_until = 0;
    return;
  }
  pairing_until = now + 120000;
  wake(now);
  screens.show(Screens::FACE);
  char text[24];
  snprintf(text, sizeof text, "code %06d", code);
  face.showTitle(text, 120000);
  face.setMood(Mood::SURPRISED, 1500);
  sound::play(talks() ? Sound::HM : Sound::CHIRP);
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
