// Goli desk bot: Goli's animated face on the Waveshare ESP32-S3-Touch-AMOLED-1.75.
// This file is the hardware: it reads the sensors and hands them to bot.cpp, which is all of Goli's
// behaviour (the browser simulator in sim/ runs the same bot.cpp).
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
#include "bot.h"
#include "config.h"
#include "goliash.h"
#include "improv.h"
#include "mic.h"
#include "motion.h"
#include "ota.h"
#include "power.h"
#include "rtc.h"
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

lv_disp_t *disp = nullptr;
power::State battery;

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

// ---- what bot.cpp needs from the hardware
void setBrightness(int level) { gfx->setBrightness(level); }

int rotation() { return int(lv_disp_get_rotation(disp)); }

void rotateFace() {
  const int rot = (rotation() + 1) % 4;
  lv_disp_set_rotation(disp, lv_disp_rot_t(rot));
  prefs.putUChar("rot", rot);
}

// A short hint under the mouth when the connection needs attention
const char *hint() {
  if (ota::updating()) return "updating myself...";
  switch (goliash::status()) {
    case goliash::Status::NO_WIFI: return "no Wi-Fi - tap me 5x";
    case goliash::Status::BAD_TOKEN: return "Goliash token? tap 5x";
    case goliash::Status::UNREACHABLE: return "Goliash unreachable";
    default: return nullptr;
  }
}

void overview(Screens &screens, uint32_t now) {
  goliash::Summary s;
  if (!goliash::summary(s)) {
    screens.setOverview(nullptr, 0, 0, 0, 0, 0,
                        settings::hasGoliash() ? "waiting for Goliash..." : "Goliash not set up (tap me 5x)");
    return;
  }
  char ago[24];
  const int32_t age = int32_t(millis() - s.at_ms);  // at_ms comes from the other core
  const int32_t min = age > 0 ? age / 60000 : 0;
  if (min) snprintf(ago, sizeof ago, "%ld min ago", (long)min);
  else snprintf(ago, sizeof ago, "just now");
  screens.setOverview(s.workspace, s.services, s.drifting, s.updates, s.exploited, s.stale, ago);
}

void statusText(char *out, size_t cap) {
  static const char *states[] = {"off", "connecting", "no Wi-Fi", "unreachable", "bad token", "ok"};
  char wifi[48], bat[32];
  if (WiFi.status() == WL_CONNECTED)
    snprintf(wifi, sizeof wifi, "%s\n%d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  else
    snprintf(wifi, sizeof wifi, "no Wi-Fi\n%s", api::hostname());
  if (battery.battery) snprintf(bat, sizeof bat, "battery %d%%%s", battery.percent, battery.charging ? " +" : "");
  else snprintf(bat, sizeof bat, "USB power");
  snprintf(out, cap, "%s\n%s\nGoliash: %s\n%s", GOLI_VERSION, wifi, states[int(goliash::status())], bat);
}

// ---- sensors -> bot
void pollTouch(uint32_t now) {
  if (!touch_irq) return;
  touch_irq = false;
  int16_t x[1], y[1];
  if (touch.getPoint(x, y, 1)) bot::touch(x[0], y[0], now);
}

void pollImu(uint32_t now) {
  static uint32_t next = 0;
  if (!imu_ok || int32_t(now - next) < 0) return;
  next = now + 20;
  float ax, ay, az;
  if (imu.getDataReady() && imu.getAccelerometer(ax, ay, az)) bot::motion(ax, ay, az, now);
}

void pollGoliash(uint32_t now) {
  goliash::Alert a;
  if (goliash::takeAlert(a)) bot::alert(a.title, a.count, a.level, goliash::quietHours(), now);
}

void pollMic(uint32_t now) {
  if (!mic_ok) return;
  if (mic::takeClap()) bot::clap(now);
  bot::loud(mic::loud());
}

void pollPower(uint32_t now) {
  static uint32_t next = 0;
  if (!power_ok || int32_t(now - next) < 0) return;
  next = now + 5000;
  battery = power::read();
  bot::power(battery.battery ? battery.percent : -1, battery.charging, battery.battery && !battery.usb);
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
  Motion motion;
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
  gfx->setBrightness(BRIGHTNESS_AWAKE);

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

  bot::Hooks hooks;
  hooks.brightness = setBrightness;
  hooks.rotation = rotation;
  hooks.rotate = rotateFace;
  hooks.setup = settings::requestSetup;  // restarts into setup mode
  hooks.hint = hint;
  hooks.overview = overview;
  hooks.status = statusText;
  bot::begin(lv_scr_act(), hooks, millis());
  api::begin();                     // before Wi-Fi starts: sets the hostname
  goliash::begin();
  improv::begin(nullptr, nullptr);  // the web installer can change the Wi-Fi
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
  bot::update(now);
  lv_timer_handler();
  delay(5);
}
