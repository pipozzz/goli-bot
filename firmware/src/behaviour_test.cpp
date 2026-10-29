// Goli's behaviour (bot.cpp) with fake time, no board (env:behaviour; CI runs it):
//   pio run -e behaviour && .pio/build/behaviour/program
// One timeline through a day: taps, focus and its break, a kitchen timer, quiet, a meeting, a knock
// on the desk, setup and demo taps, falling asleep, and the next morning's greeting.
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vector>

#include "bot.h"
#include "config.h"
#include "demo.h"
#include "sound.h"

uint32_t g_fake_millis = 0;

namespace {

time_t wall = 0;  // Unix seconds, moves with g_fake_millis
std::vector<Sound> played;
bool setup_called = false, quiet_hours = false, buddy_on = false;
int brightness = -1;
int failures = 0, checks = 0;

#define CHECK(cond, ...)                          \
  do {                                            \
    checks++;                                     \
    if (!(cond)) {                                \
      failures++;                                 \
      printf("FAIL line %d: ", __LINE__);         \
      printf(__VA_ARGS__);                        \
      printf("\n");                               \
    }                                             \
  } while (0)

lv_color_t buf[LCD_SIZE * 20];
void flush(lv_disp_drv_t *drv, const lv_area_t *, lv_color_t *) { lv_disp_flush_ready(drv); }

// Time passes: the main loop every 10 ms, LVGL drawn now and then
void advance(uint32_t ms) {
  static uint32_t frac = 0;
  for (uint32_t t = 0; t < ms; t += 10) {
    g_fake_millis += 10;
    frac += 10;
    if (frac >= 1000) {
      frac -= 1000;
      wall++;
    }
    lv_tick_inc(10);
    bot::update(g_fake_millis);
    if (g_fake_millis % 500 == 0) lv_timer_handler();
  }
}

// A finger on the glass for `ms`, the panel reporting every 20 ms
void press(uint32_t ms, int x = 233, int y = 233) {
  for (uint32_t t = 0; t < ms; t += 20) {
    bot::touch(x, y, g_fake_millis);
    advance(20);
  }
  bot::lift(g_fake_millis);
}

void taps(int n) {
  for (int i = 0; i < n; i++) {
    press(60);
    advance(200);
  }
  advance(700);  // the sequence is over
}

bool heard(Sound s) {
  for (Sound p : played)
    if (p == s) return true;
  return false;
}

bot::State st() { return bot::state(g_fake_millis); }

// Gravity at rest (the board upright: g along +x), with an optional jolt on z
void imu(float extra_z = 0) { bot::motion(1.0f, 0.0f, extra_z, g_fake_millis); }
void still(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 20) {
    imu();
    advance(20);
  }
}

time_t localTime(int y, int mo, int d, int h, int mi) {
  struct tm t = {};
  t.tm_year = y - 1900, t.tm_mon = mo - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = mi, t.tm_isdst = -1;
  return mktime(&t);
}

}  // namespace

// The test's clock replaces bot.cpp's (weak) one
time_t bot::wallClock() { return wall; }

// sound.h without a speaker: remember what he played
namespace sound {
bool begin() { return true; }
void play(Sound s) { played.push_back(s); }
void setVolume(int) {}
void stop() {}
bool playing() { return false; }
bool readMic(int16_t *, size_t) { return false; }
bool setCustom(const int16_t *, size_t) { return false; }
size_t customSamples() { return 0; }
}  // namespace sound

int main() {
  setenv("TZ", GOLI_TZ, 1);
  tzset();
  wall = localTime(2026, 10, 29, 7, 50);  // a Thursday morning

  lv_init();
  static lv_disp_draw_buf_t db;
  lv_disp_draw_buf_init(&db, buf, nullptr, LCD_SIZE * 20);
  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = drv.ver_res = LCD_SIZE;
  drv.flush_cb = flush;
  drv.draw_buf = &db;
  lv_disp_drv_register(&drv);

  bot::Hooks hooks;
  hooks.brightness = [](int level) { brightness = level; };
  hooks.setup = [] { setup_called = true; };
  hooks.quietHours = [] { return quiet_hours; };
  hooks.buddy = [] { return buddy_on; };
  bot::begin(lv_scr_act(), hooks, g_fake_millis);
  advance(1000);

  // ---- taps
  played.clear();
  taps(1);
  advance(600);
  CHECK(heard(Sound::BOOP) && heard(Sound::CHIRP), "a tap: boop, then a happy chirp");
  played.clear();
  taps(2);
  CHECK(!strcmp(st().mood, "wink") && heard(Sound::CHIRP), "two taps: wink (%s)", st().mood);

  // ---- alerts
  played.clear();
  advance(2000);
  bot::alert("checkout 2.4.1", 1, AlertLevel::INFO, false, g_fake_millis);
  CHECK(heard(Sound::DING) && st().unseen == 1, "an alert dings and shows a badge");
  bot::alert("payments drift", 1, AlertLevel::CRITICAL, false, g_fake_millis);
  CHECK(heard(Sound::ALARM) && !strcmp(st().mood, "alarmed") && st().unseen == 2, "critical: ding-ding, alarmed");
  advance(5000);
  taps(1);  // seen
  CHECK(st().unseen == 0, "a tap marks alerts seen");
  played.clear();
  bot::alert("at night", 1, AlertLevel::INFO, true, g_fake_millis);
  CHECK(played.empty() && st().unseen == 1, "quiet hours: badge only");
  taps(1);

  // ---- focus, then its break
  advance(3000);
  taps(3);
  CHECK(st().focus, "three taps: focus on");
  played.clear();
  bot::alert("not now", 1, AlertLevel::INFO, false, g_fake_millis);
  CHECK(!heard(Sound::DING), "focusing: an ordinary alert stays quiet");
  bot::alert("on fire", 1, AlertLevel::CRITICAL, false, g_fake_millis);
  CHECK(heard(Sound::ALARM), "focusing: critical still rings");
  taps(1);
  played.clear();
  advance(FOCUS_MIN * 60 * 1000UL + 2000);
  CHECK(!st().focus && heard(Sound::CHIRP), "after 25 min: focus over, a cheer");
  played.clear();
  advance(5 * 60 * 1000UL + 2000);
  CHECK(heard(Sound::DING), "after the 5 min break: back to it");
  taps(3);  // focus on...
  taps(3);  // ...and off again
  CHECK(!st().focus, "three taps again: focus off");

  // ---- kitchen timer, through quiet
  bot::quiet(true);
  CHECK(st().quiet, "quiet on");
  played.clear();
  bot::alert("hush", 1, AlertLevel::INFO, false, g_fake_millis);
  CHECK(played.empty(), "quiet: badge only");
  bot::timer(1, g_fake_millis);
  CHECK(st().timer_s > 50, "timer running (%d s)", st().timer_s);
  played.clear();
  advance(61 * 1000);
  CHECK(st().timer_s == 0 && heard(Sound::DING), "the timer rings even when quiet");
  bot::quiet(false);
  taps(1);

  // ---- a meeting on now
  bot::Meeting m;
  snprintf(m.title, sizeof m.title, "stand-up");
  m.start = int64_t(wall) - 60;
  m.end = int64_t(wall) + 30 * 60;
  bot::meetings(&m, 1);
  advance(1500);
  CHECK(st().in_meeting, "in a meeting");
  played.clear();
  bot::alert("fyi", 1, AlertLevel::INFO, false, g_fake_millis);
  CHECK(!heard(Sound::DING), "in a meeting: ordinary alerts stay quiet");
  bot::alert("prod down", 1, AlertLevel::CRITICAL, false, g_fake_millis);
  CHECK(heard(Sound::ALARM), "in a meeting: critical still rings");
  taps(1);
  advance(31 * 60 * 1000UL);
  CHECK(!st().in_meeting, "the meeting is over");

  // ---- a meeting coming up: a ding when it starts
  m.start = int64_t(wall) + 90;
  m.end = m.start + 900;
  bot::meetings(&m, 1);
  played.clear();
  advance(100 * 1000);
  CHECK(heard(Sound::DING), "the meeting starts: ding");
  bot::meetings(nullptr, 0);

  // ---- knock knock: snooze
  still(3000);  // he learns which way is up
  imu(0.5f);
  advance(20);
  still(300);
  imu(0.5f);
  advance(20);
  still(200);
  played.clear();
  bot::alert("snoozed?", 1, AlertLevel::INFO, false, g_fake_millis);
  CHECK(!heard(Sound::DING), "two knocks on the desk: alerts snoozed");
  advance(10 * 60 * 1000UL + 1000);
  taps(1);
  played.clear();
  bot::alert("awake again", 1, AlertLevel::INFO, false, g_fake_millis);
  CHECK(heard(Sound::DING), "the snooze is over after 10 minutes");
  taps(1);

  // ---- 5 taps: setup; 10 taps: demo
  taps(SETUP_TAPS);
  CHECK(setup_called, "five taps: setup mode");
  taps(DEMO_TAPS);
  CHECK(demo::active(), "ten taps: demo");
  taps(1);
  CHECK(!demo::active(), "a tap ends the demo");

  // ---- falling asleep, and waking up
  played.clear();
  still(20);
  advance(ASLEEP_AFTER_MS + 5000);
  CHECK(st().asleep && heard(Sound::YAWN) && brightness == BRIGHTNESS_ASLEEP, "idle: asleep with a yawn, dimmed");
  press(60);
  advance(100);
  CHECK(!st().asleep && brightness == BRIGHTNESS_AWAKE, "a touch wakes him");

  // ---- the next morning: good morning
  buddy_on = true;
  advance(4 * 60 * 1000UL);  // nobody around for a while
  CHECK(!st().present, "nobody at the desk");
  wall = localTime(2026, 10, 30, 8, 0);
  played.clear();
  advance(2000);  // the new day
  CHECK(played.empty(), "a new morning, but nobody to greet");
  bot::presence(g_fake_millis);
  advance(2000);
  CHECK(heard(Sound::CHIRP), "first person at the desk in the morning: good morning");
  played.clear();
  bot::presence(g_fake_millis);
  advance(2000);
  CHECK(!heard(Sound::CHIRP), "only once a day");

  printf("%d behaviour checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
