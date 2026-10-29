// Goli in the browser: the firmware's bot.cpp, face and pages compiled to WebAssembly.
// The mouse is the finger, sliders and buttons on the page stand in for the IMU, the
// microphones, the battery and Goliash; sounds come from the same synth as on the board.
#include <emscripten.h>
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bot.h"
#include "config.h"
#include "sound.h"
#include "synth.h"
#include "text.h"

namespace {

uint32_t fb[LCD_SIZE * LCD_SIZE];  // RGBA, read by the page
bool dirty = true;
lv_disp_t *disp = nullptr;
lv_color_t buf[LCD_SIZE * 60];

bool finger = false;
int fx = 0, fy = 0;
float roll = 0, pitch = 0;  // tilt sliders, radians
bool flat = false, noisy = false;
uint32_t shake_until = 0, bump_at = 0;
const char *hint_text = nullptr;
char hint_buf[64];
Sound sounds[3] = {Sound::CHIRP, Sound::DING, Sound::ALARM};  // by AlertLevel: good, info, critical
bool talk = false, buddy_on = true, at_desk = false, bday = false;
int break_min = 50, focus_min = FOCUS_MIN;
int face_clock = 1, face_clock_min = 1;  // now and then: every minute here, to see it soon
bool fahr = false;
uint8_t weather_mask = 0;
char today[6] = "";

void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  for (int y = a->y1; y <= a->y2; y++) {
    for (int x = a->x1; x <= a->x2; x++, px++) {
      const uint16_t c = px->full;
      const uint32_t r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
      fb[y * LCD_SIZE + x] = 0xff000000u | (b << 16) | (g << 8) | r;
    }
  }
  dirty = true;
  lv_disp_flush_ready(drv);
}

EM_JS(void, js_brightness, (int level), { if (Module.onBrightness) Module.onBrightness(level); });
EM_JS(void, js_setup, (), { if (Module.onSetup) Module.onSetup(); });
EM_JS(void, js_play, (const int16_t *data, int n, int rate), {
  if (Module.onSound) Module.onSound(HEAP16.subarray(data >> 1, (data >> 1) + n), rate);
});
EM_JS(void, js_stop, (), { if (Module.onStopSound) Module.onStopSound(); });

int rotation() { return int(lv_disp_get_rotation(disp)); }
void rotate() { lv_disp_set_rotation(disp, lv_disp_rot_t((rotation() + 1) % 4)); }
const char *hint() { return hint_text; }

Sound alertSound(AlertLevel level) { return sounds[int(level)]; }
bool talks() { return talk; }
bool buddy() { return buddy_on; }
int breakMin() { return break_min; }
int focusMin() { return focus_min; }
int faceClock() { return face_clock; }
int faceClockMin() { return face_clock_min; }
bool fahrenheit() { return fahr; }
int temperature() {
  return weather_mask & HOT ? 31 : weather_mask & COLD ? -3 : weather_mask & UMBRELLA ? 9 : weather_mask & SUNGLASSES ? 24 : 14;
}
void weatherText(char *out, size_t cap) {
  tempText(out, cap, temperature(), fahr);
  if (weather_mask & UMBRELLA) snprintf(out + strlen(out), cap - strlen(out), ", rain later");
}
const char *birthday() {
  if (!bday) return nullptr;
  const time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(today, sizeof today, "%02d-%02d", lt.tm_mon + 1, lt.tm_mday);
  return today;
}

void overview(Screens &s, uint32_t) { s.setOverview("acme", 42, 2, 7, 1, 0, "simulated"); }

void status(char *out, size_t cap) {
  snprintf(out, cap, "simulator\n%s\nGoliash: %s\nbrowser power", GOLI_VERSION, hint_text ? "trouble" : "ok");
}

// A fake accelerometer: gravity from the tilt sliders, plus shaking and bumps
void feedMotion(uint32_t now) {
  static uint32_t next = 0;
  if (int32_t(now - next) < 0) return;
  next = now + 20;
  float ax, ay, az;
  if (flat) {
    ax = 0.02f, ay = 0.f, az = 1.f;
  } else {
    ax = cosf(roll) * cosf(pitch), ay = sinf(roll) * cosf(pitch), az = sinf(pitch);
  }
  if (int32_t(shake_until - now) > 0) {
    ax += (rand() % 200 - 100) / 60.f;
    ay += (rand() % 200 - 100) / 60.f;
  }
  if (bump_at && now - bump_at < 40) az += 0.9f;
  bot::motion(ax, ay, az, now);
}

void frame() {
  static uint32_t last = uint32_t(emscripten_get_now());
  const uint32_t now = uint32_t(emscripten_get_now());
  lv_tick_inc(now - last);
  last = now;
  if (finger) bot::touch(fx, fy, now);  // the panel keeps reporting while a finger is down
  feedMotion(now);
  bot::loud(noisy);
  static uint32_t next_presence = 0;
  if (at_desk && int32_t(now - next_presence) >= 0) {
    next_presence = now + 1000;
    bot::presence(now);
  }
  bot::update(now);
  lv_timer_handler();
}

}  // namespace

// ---- sound.h for the browser: render with the board's synth and hand it to Web Audio
namespace sound {
bool begin() { return true; }
void play(Sound s) {
  static int16_t pcm[synth::kRate * 3];
  int n = 0;
  while (n + 256 <= int(sizeof pcm / sizeof *pcm) && synth::render(s, n, 256, pcm + n)) n += 256;
  js_play(pcm, n + 256 <= int(sizeof pcm / sizeof *pcm) ? n + 256 : n, synth::kRate);
}
void setVolume(int) {}
void stop() { js_stop(); }
bool playing() { return false; }
bool readMic(int16_t *, size_t) { return false; }
}  // namespace sound

extern "C" {

EMSCRIPTEN_KEEPALIVE uint32_t *sim_fb() { return fb; }
EMSCRIPTEN_KEEPALIVE int sim_take_dirty() {
  const bool d = dirty;
  dirty = false;
  return d;
}
EMSCRIPTEN_KEEPALIVE void sim_pointer(int x, int y, int down) {
  // Every pointer event is a panel sample, the last one included: a quick drag still ends where it ended
  const uint32_t now = uint32_t(emscripten_get_now());
  if (down || finger) bot::touch(x, y, now);
  if (!down && finger) bot::lift(now);  // browsers may run frames slowly: don't wait for one to notice
  finger = down;
  fx = x;
  fy = y;
}
EMSCRIPTEN_KEEPALIVE void sim_tilt(float roll_deg, float pitch_deg) {
  roll = roll_deg * 3.14159265f / 180.f;
  pitch = pitch_deg * 3.14159265f / 180.f;
}
EMSCRIPTEN_KEEPALIVE void sim_flat(int on) { flat = on; }
EMSCRIPTEN_KEEPALIVE void sim_shake() { shake_until = uint32_t(emscripten_get_now()) + 1200; }
EMSCRIPTEN_KEEPALIVE void sim_bump() { bump_at = uint32_t(emscripten_get_now()); }
EMSCRIPTEN_KEEPALIVE void sim_clap() { bot::clap(uint32_t(emscripten_get_now())); }
EMSCRIPTEN_KEEPALIVE void sim_noise(int on) { noisy = on; }
EMSCRIPTEN_KEEPALIVE void sim_alert(int level, const char *title, int prod_deploy) {
  bot::alert(title, 1, AlertLevel(level), false, uint32_t(emscripten_get_now()), prod_deploy);
}
EMSCRIPTEN_KEEPALIVE void sim_sound(int level, const char *name) {
  sounds[level] = soundByName(name, sounds[level]);
}
EMSCRIPTEN_KEEPALIVE void sim_play(const char *name) { sound::play(soundByName(name, Sound::NONE)); }
EMSCRIPTEN_KEEPALIVE int sim_sound_count() { return kSoundCount; }
EMSCRIPTEN_KEEPALIVE const char *sim_sound_name(int i) { return kSoundNames[i].name; }
EMSCRIPTEN_KEEPALIVE const char *sim_sound_label(int i) { return kSoundNames[i].label; }
EMSCRIPTEN_KEEPALIVE void sim_talk(int on) { talk = on; }
EMSCRIPTEN_KEEPALIVE void sim_meeting(const char *title, int in_s) {  // 30 minutes long; in_s < 0: already on
  bot::Meeting m;
  snprintf(m.title, sizeof m.title, "%s", title);
  m.start = int64_t(time(nullptr)) + in_s;
  m.end = m.start + 1800;
  bot::meetings(&m, 1);
}
EMSCRIPTEN_KEEPALIVE void sim_weather(int mask) {
  weather_mask = uint8_t(mask);
  bot::weather(weather_mask, temperature());
}
EMSCRIPTEN_KEEPALIVE void sim_birthday(int on) {
  bday = on;
  bot::weather(weather_mask, temperature());  // dresses him again, party hat included
}
EMSCRIPTEN_KEEPALIVE void sim_face_clock(int mode) { face_clock = mode; }
EMSCRIPTEN_KEEPALIVE void sim_fahrenheit(int on) { fahr = on; }
EMSCRIPTEN_KEEPALIVE void sim_desk(int on, int minutes) {
  at_desk = on;
  break_min = minutes;
}
EMSCRIPTEN_KEEPALIVE void sim_focus_min(int m) { focus_min = m; }
EMSCRIPTEN_KEEPALIVE void sim_lang(const char *code) { setLanguage(code); }
EMSCRIPTEN_KEEPALIVE void sim_timer(int minutes) { bot::timer(minutes, uint32_t(emscripten_get_now())); }
EMSCRIPTEN_KEEPALIVE void sim_quiet(int on) { bot::quiet(on); }
EMSCRIPTEN_KEEPALIVE void sim_knock() { bot::knock(uint32_t(emscripten_get_now())); }
EMSCRIPTEN_KEEPALIVE void sim_team(int dir) { bot::teamAlert(dir, uint32_t(emscripten_get_now())); }
EMSCRIPTEN_KEEPALIVE void sim_yesterday(int alerts) { bot::yesterday(alerts, 0); }
EMSCRIPTEN_KEEPALIVE void sim_goliash(int on) { bot::pages(on); }
EMSCRIPTEN_KEEPALIVE void sim_power(int percent, int charging, int on_battery) {
  bot::power(percent, charging, on_battery);
}
EMSCRIPTEN_KEEPALIVE void sim_hint(const char *text) {
  if (!text || !*text) {
    hint_text = nullptr;
    return;
  }
  snprintf(hint_buf, sizeof hint_buf, "%s", text);
  hint_text = hint_buf;
}

}  // extern "C"

int main() {
  srand(unsigned(emscripten_get_now()));
  lv_init();
  static lv_disp_draw_buf_t db;
  lv_disp_draw_buf_init(&db, buf, nullptr, LCD_SIZE * 60);
  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = drv.ver_res = LCD_SIZE;
  drv.flush_cb = flush;
  drv.draw_buf = &db;
  drv.sw_rotate = 1;
  disp = lv_disp_drv_register(&drv);

  bot::Hooks hooks;
  hooks.brightness = js_brightness;
  hooks.rotation = rotation;
  hooks.rotate = rotate;
  hooks.setup = js_setup;
  hooks.hint = hint;
  hooks.overview = overview;
  hooks.status = status;
  hooks.alertSound = alertSound;
  hooks.talks = talks;
  hooks.buddy = buddy;
  hooks.breakMin = breakMin;
  hooks.birthday = birthday;
  hooks.focusMin = focusMin;
  hooks.weather = weatherText;
  hooks.faceClock = faceClock;
  hooks.faceClockMin = faceClockMin;
  hooks.fahrenheit = fahrenheit;
  bot::begin(lv_scr_act(), hooks, uint32_t(emscripten_get_now()));
  bot::weather(weather_mask, temperature());  // a mild 14 C to start with
  bot::meetings(nullptr, 0);
  emscripten_set_main_loop(frame, 0, false);
  return 0;
}
