// Goli desk bot: Goli's animated face on the Waveshare ESP32-S3-Touch-AMOLED-1.75.
//
// Touch: tap = boop (surprised, then happy), 2 taps = wink, 3 taps = focus mode, 5 taps = setup,
//        10 taps = demo; finger on the face = petting; swipe = overview / clock / status pages;
//        hold still for ROTATE_HOLD_MS = turn the face 90° (saved).
// Ears:  a clap marks alerts as seen; loud noise makes him wince.
// IMU:   tilt = pupils follow, shake = dizzy, bump = surprised, lying flat = falls asleep.
// Idle:  tired -> drowsy -> asleep (dim); any touch or motion wakes him up.
// Boot with a finger on the face: self-test screen.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <time.h>

#include "Arduino_GFX_Library.h"
#include "SensorQMI8658.hpp"
#include "TouchDrvCSTXXX.hpp"
#include "api.h"
#include "config.h"
#include "demo.h"
#include "face.h"
#include "goliash.h"
#include "improv.h"
#include "mic.h"
#include "motion.h"
#include "ota.h"
#include "power.h"
#include "rtc.h"
#include "screens.h"
#include "selftest_ui.h"
#include "settings.h"
#include "setup.h"
#include "sound.h"

namespace {

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *gfx = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_SIZE, LCD_SIZE, 6, 0, 0, 0);
TouchDrvCST92xx touch;
SensorQMI8658 imu;
Preferences prefs;

Face face;
Screens screens;
Motion motion;
lv_disp_t *disp = nullptr;

const int kBufLines = 40;
lv_disp_draw_buf_t draw_buf;

volatile bool touch_irq = false;
bool touch_ok = false, imu_ok = false, sound_ok = false, power_ok = false, mic_ok = false;

// CO5300 wants even start and odd end coordinates
void rounder(lv_disp_drv_t *, lv_area_t *a) {
  if (a->x1 & 1) a->x1--;
  if (a->y1 & 1) a->y1--;
  if (!(a->x2 & 1)) a->x2++;
  if (!(a->y2 & 1)) a->y2++;
}

void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  gfx->draw16bitRGBBitmap(a->x1, a->y1, (uint16_t *)&px->full, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1);
  lv_disp_flush_ready(drv);
}

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
uint32_t happy_after = 0;     // boop: surprised first, then happy
bool asleep = false;
uint32_t asleep_since = 0;
power::State battery;

void setBrightness(int want) {
  static int current = -1;
  if (want != current) gfx->setBrightness(current = want);
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
  for (int r = int(lv_disp_get_rotation(disp)); r > 0; r--) {
    const int t = dx;  // same mapping as LVGL's input rotation
    dx = -dy;
    dy = t;
  }
  const int along = abs(dx) >= abs(dy) ? dx : dy;
  screens.show(screens.page() + (along < 0 ? 1 : -1));
}

void rotateFace() {
  const int rot = (int(lv_disp_get_rotation(disp)) + 1) % 4;
  lv_disp_set_rotation(disp, lv_disp_rot_t(rot));
  prefs.putUChar("rot", rot);
  face.setMood(Mood::SURPRISED, 800);
}

void pollTouch(uint32_t now) {
  if (touch_irq) {
    touch_irq = false;
    int16_t x[1], y[1];
    if (touch.getPoint(x, y, 1)) {
      if (!tp.down) {
        tp = {};
        tp.down = true;
        tp.start = now;
        tp.x0 = x[0];
        tp.y0 = y[0];
      }
      tp.travel = max<int16_t>(tp.travel, abs(x[0] - tp.x0) + abs(y[0] - tp.y0));
      tp.x1 = x[0];
      tp.y1 = y[0];
      tp.last = now;
      page_since = now;
      wake(now);
    }
  }
  if (tp.down) {
    const uint32_t held = now - tp.start;
    if (now - tp.last > 120) {  // finger lifted
      tp.down = false;
      face.setPetting(false);
      const int dx = tp.x1 - tp.x0, dy = tp.y1 - tp.y0;
      if (!tp.petting && !tp.rotated && held < 350 && tp.travel < 30) {
        taps = now - last_tap < 500 ? taps + 1 : 1;
        last_tap = now;
        return;
      }
      taps = 0;  // a swipe, a hold or petting is not part of a tap sequence
      if (!tp.petting && !tp.rotated && held < 600 && max(abs(dx), abs(dy)) > 100) {
        if (demo::active()) demo::stop(face, resting());
        swipe(dx, dy);
      }
      return;
    }
    if (!tp.rotated && tp.travel < 25 && held > ROTATE_HOLD_MS) {
      tp.rotated = true;
      face.setPetting(false);
      rotateFace();
    } else if (!tp.rotated && held > 500 && screens.page() == Screens::FACE) {
      tp.petting = true;
      face.setPetting(true);
    }
  }
  if (taps && !tp.down && now - last_tap > 500) {  // the tap sequence is over
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
    if (n >= SETUP_TAPS) settings::requestSetup();  // restarts into setup mode
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
}

void pollImu(uint32_t now) {
  static uint32_t next = 0;
  if (now < next) return;
  next = now + 20;
  float ax, ay, az;
  if (!imu.getDataReady() || !imu.getAccelerometer(ax, ay, az)) return;
  motion.feed(ax, ay, az, now);
  face.setTilt(motion.tiltX(), motion.tiltY());

  if (motion.shaking()) {
    wake(now);
    if (face.mood() != Mood::DIZZY) sound::play(Sound::WOBBLE);
    face.setMood(Mood::DIZZY, 2500);
  } else if (motion.takeJolt()) {
    wake(now);
    if (face.mood() != Mood::DIZZY) face.setMood(Mood::SURPRISED, 600);
  }
}

void pollGoliash(uint32_t now) {
  goliash::Alert a;
  if (demo::active() || !goliash::takeAlert(a)) return;
  face.showAlert(a.title, face.unseen() + a.count, a.level);
  if (goliash::quietHours()) return;  // at night: badge only, let him (and you) sleep
  if (focus_until && a.level != AlertLevel::CRITICAL) return;  // focusing: only what's on fire
  screens.show(Screens::FACE);
  wake(now);
  switch (a.level) {
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

// Claps and noise
void pollMic(uint32_t now) {
  if (!mic_ok || demo::active()) return;
  const bool touched = tp.down || now - tp.last < 400;  // a knock on the glass is not a clap
  if (mic::takeClap() && !touched) {
    wake(now);
    if (face.unseen()) {  // "got it": alerts seen
      face.clearAlert();
      face.setMood(Mood::WINK, 1000);
    } else {
      face.setMood(Mood::SURPRISED, 600);
    }
  }
  // Wince while it's loud, but never over an alert's face or anything else that is showing
  if (mic::loud() && !asleep && (face.mood() == Mood::WINCE || !face.held())) face.setMood(Mood::WINCE, 800);
}

// Once a second: focus countdown, the night clock and the swipe pages
void tickSecond(uint32_t now) {
  static uint32_t next = 0;
  if (now < next) return;
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

  const time_t t = time(nullptr);
  const bool clock_ok = t > 1700000000;
  struct tm lt;
  localtime_r(&t, &lt);
  char hhmm[8], date[32];
  strftime(hhmm, sizeof hhmm, "%H:%M", &lt);
  strftime(date, sizeof date, "%a %d %b", &lt);
  face.setClock(clock_ok ? hhmm : nullptr);

  switch (screens.page()) {
    case Screens::OVERVIEW: {
      goliash::Summary s;
      if (goliash::summary(s)) {
        char ago[24];
        const int32_t age = int32_t(millis() - s.at_ms);  // at_ms comes from the other core
        const int32_t min = age > 0 ? age / 60000 : 0;
        if (min) snprintf(ago, sizeof ago, "%ld min ago", (long)min);
        else snprintf(ago, sizeof ago, "just now");
        screens.setOverview(s.workspace, s.services, s.drifting, s.updates, s.exploited, s.stale, ago);
      } else {
        screens.setOverview(nullptr, 0, 0, 0, 0, 0,
                            settings::hasGoliash() ? "waiting for Goliash..." : "Goliash not set up (tap me 5x)");
      }
      break;
    }
    case Screens::CLOCK:
      screens.setClock(clock_ok ? hhmm : "--:--", clock_ok ? date : "no time yet");
      break;
    case Screens::STATUS: {
      static const char *states[] = {"off", "connecting", "no Wi-Fi", "unreachable", "bad token", "ok"};
      char text[256], wifi[48], bat[32];
      if (WiFi.status() == WL_CONNECTED)
        snprintf(wifi, sizeof wifi, "%s\n%d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      else
        snprintf(wifi, sizeof wifi, "no Wi-Fi\n%s", api::hostname());
      if (battery.battery) snprintf(bat, sizeof bat, "battery %d%%%s", battery.percent, battery.charging ? " +" : "");
      else snprintf(bat, sizeof bat, "USB power");
      snprintf(text, sizeof text, "%s\n%s\nGoliash: %s\n%s", GOLI_VERSION, wifi,
               states[int(goliash::status())], bat);
      screens.setStatus(text);
      break;
    }
    default:
      break;
  }
  if (screens.page() != Screens::FACE && now - page_since > PAGE_TIMEOUT_MS) screens.show(Screens::FACE);
}

// Battery icon while charging or low, and a dark screen when he sleeps on battery for long
void pollPower(uint32_t now) {
  static uint32_t next = 0;
  if (!power_ok || now < next) return;
  next = now + 5000;
  battery = power::read();
  const bool low = battery.battery && !battery.usb && battery.percent >= 0 && battery.percent < BATTERY_LOW_PCT;
  face.showBattery(battery.charging || low ? battery.percent : -1, battery.charging);
}

// A short hint under the mouth when the connection needs attention
void showStatus(uint32_t now) {
  static uint32_t next = 0;
  if (now < next) return;
  next = now + 1000;
  const char *msg = nullptr;
  if (ota::updating()) {
    face.showStatus("updating myself...");
    return;
  }
  if (demo::active()) return;  // the demo shows its own hint
  switch (goliash::status()) {
    case goliash::Status::NO_WIFI: msg = "no Wi-Fi - tap me 5x"; break;
    case goliash::Status::BAD_TOKEN: msg = "Goliash token? tap 5x"; break;
    case goliash::Status::UNREACHABLE: msg = "Goliash unreachable"; break;
    default: break;
  }
  face.showStatus(msg);
}

void updateIdle(uint32_t now) {
  if (happy_after && now >= happy_after) {
    happy_after = 0;
    face.setMood(Mood::HAPPY, 2200);
    sound::play(Sound::CHIRP);
  }
  static uint32_t flat_since = 0;
  flat_since = motion.lyingFlat() ? (flat_since ? flat_since : now) : 0;
  const uint32_t idle = now - max(last_activity, motion.lastMoveMs());
  if (focus_until || demo::active()) return;  // no napping on the job

  if (asleep && battery.battery && !battery.usb && now - asleep_since > SCREEN_OFF_AFTER_MS) {
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

// True when a finger stays on the face for SELFTEST_HOLD_MS while Goli starts
bool fingerAtBoot() {
  int16_t x[1], y[1];
  if (!touch_ok || !touch.getPoint(x, y, 1)) return false;
  for (const uint32_t t0 = millis(); millis() - t0 < SELFTEST_HOLD_MS; delay(50)) {
    if (!touch.getPoint(x, y, 1)) return false;
  }
  return true;
}

// Every sensor live on one screen, for checking a freshly assembled Goli. Never returns.
[[noreturn]] void selfTest() {
  lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);  // raw panel coordinates: the dot must sit under the finger
  SelfTestUi ui;
  ui.begin(lv_scr_act(), GOLI_VERSION);
  sound::play(Sound::CHIRP);
  if (settings::hasWifi()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings::get().ssid.c_str(), settings::get().pass.c_str());
  }
  ui.setLine(0, touch_ok ? "touch: lift, then tap" : "touch: no answer", touch_ok);
  ui.setLine(2, sound_ok ? "sound: tap for a ding" : "sound: no codec", sound_ok);
  if (!mic_ok) ui.setLine(5, "mic: no ADC", false);

  bool armed = false, down = false;  // armed once the boot finger is lifted
  uint32_t down_since = 0, last_seen = 0, last = millis(), next_slow = 0, next_imu = 0;
  int dings = 0;
  char line[64];
  for (;;) {
    const uint32_t now = millis();
    lv_tick_inc(now - last);
    last = now;

    int16_t x[1], y[1];
    if (touch_ok && touch.getPoint(x, y, 1)) {
      if (!down) down_since = now;
      down = true;
      last_seen = now;
      ui.setTouch(true, x[0], y[0]);
      snprintf(line, sizeof line, "touch: %d, %d", x[0], y[0]);
      ui.setLine(0, line, true);
      if (armed && now - down_since > 3000) ESP.restart();
    } else if (down && now - last_seen > 120) {
      down = false;
      ui.setTouch(false, 0, 0);
      if (armed && now - down_since < 400) {
        sound::play(Sound::DING);
        snprintf(line, sizeof line, "sound: ding #%d", ++dings);
        ui.setLine(2, line, sound_ok);
      }
      armed = true;
    } else if (!down) {
      armed = true;
    }

    if (imu_ok && now >= next_imu) {
      next_imu = now + 20;
      float ax, ay, az;
      if (imu.getDataReady() && imu.getAccelerometer(ax, ay, az)) {
        motion.feed(ax, ay, az, now);
        ui.setTilt(motion.tiltX(), motion.tiltY());
      }
    }

    if (now >= next_slow) {
      next_slow = now + 1000;
      if (imu_ok) snprintf(line, sizeof line, "tilt: %+.2f  %+.2f", motion.tiltX(), motion.tiltY());
      ui.setLine(1, imu_ok ? line : "tilt: no IMU", imu_ok);

      if (!settings::hasWifi()) {
        ui.setLine(3, "Wi-Fi: not set up", false);
      } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(line, sizeof line, "Wi-Fi: %d dBm", WiFi.RSSI());
        ui.setLine(3, line, true);
      } else {
        ui.setLine(3, "Wi-Fi: joining...", now < 20000);
      }

      if (mic_ok) {
        snprintf(line, sizeof line, "mic: %d dB", mic::levelDb());
        ui.setLine(5, line, true);
      }

      const power::State p = power::read();
      if (!power_ok) snprintf(line, sizeof line, "power: no PMU");
      else if (p.battery) snprintf(line, sizeof line, "battery: %d%% %.2f V%s", p.percent, p.millivolts / 1000.f, p.charging ? " +" : "");
      else snprintf(line, sizeof line, "USB, no battery");
      ui.setLine(4, line, power_ok);
    }
    lv_timer_handler();
    delay(5);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin(IIC_SDA, IIC_SCL);
  rtc::begin();  // the time, even before Wi-Fi
  randomSeed(esp_random());
  prefs.begin("goli", false);
  settings::load();
  const bool want_setup = settings::takeSetupRequest() || !settings::hasWifi();

  if (!gfx->begin()) Serial.println("display init failed");
  gfx->fillScreen(0x0000);
  setBrightness(true);

  touch.setPins(TP_RESET, TP_INT);
  touch_ok = touch.begin(Wire, CST92XX_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (touch_ok) {
    touch.setMaxCoordinates(LCD_SIZE, LCD_SIZE);
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TP_INT), [] { touch_irq = true; }, FALLING);
  } else {
    Serial.println("touch init failed");
  }

  imu_ok = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (imu_ok) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  } else {
    Serial.println("IMU init failed");
  }

  sound_ok = sound::begin();
  if (!sound_ok) Serial.println("audio init failed");
  mic_ok = sound_ok && mic::begin();
  if (!mic_ok) Serial.println("mic init failed");
  power_ok = power::begin();
  if (!power_ok) Serial.println("PMU init failed");
  const bool want_selftest = fingerAtBoot();

  lv_init();
  const size_t buf_px = LCD_SIZE * kBufLines;
  auto *b1 = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  auto *b2 = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_disp_draw_buf_init(&draw_buf, b1, b2, buf_px);

  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = LCD_SIZE;
  drv.ver_res = LCD_SIZE;
  drv.flush_cb = flush;
  drv.rounder_cb = rounder;
  drv.draw_buf = &draw_buf;
  drv.sw_rotate = 1;
  drv.rotated = lv_disp_rot_t(prefs.getUChar("rot", LV_DISP_ROT_NONE) % 4);
  disp = lv_disp_drv_register(&drv);

  if (want_selftest) selfTest();      // never returns
  if (want_setup) setup_mode::run();  // never returns

  face.begin(lv_scr_act());
  screens.begin(lv_scr_act());  // on top of the face
  api::begin();                 // before Wi-Fi starts: sets the hostname
  goliash::begin();
  improv::begin(nullptr, nullptr);  // the web installer can change the Wi-Fi
  last_activity = millis();
  Serial.printf("Goli %s is up. Furt dačo!\n", GOLI_VERSION);
}

void loop() {
  static uint32_t last_tick = millis();
  const uint32_t now = millis();
  lv_tick_inc(now - last_tick);
  last_tick = now;

  pollTouch(now);
  pollImu(now);
  pollGoliash(now);
  pollMic(now);
  pollPower(now);
  improv::loop();
  rtc::maintain(now);
  demo::update(now);
  tickSecond(now);
  showStatus(now);
  updateIdle(now);
  face.update(now);
  lv_timer_handler();
  delay(5);
}
