// Goli desk bot: Goli's animated face on the Waveshare ESP32-S3-Touch-AMOLED-1.75.
//
// Touch: tap = boop (surprised, then happy), double tap = wink, finger on the face = petting,
//        hold still for ROTATE_HOLD_MS = turn the face 90° (saved).
// IMU:   tilt = pupils follow, shake = dizzy, bump = surprised, lying flat = falls asleep.
// Idle:  tired -> drowsy -> asleep (dim); any touch or motion wakes him up.
// Boot with a finger on the face: self-test screen.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "Arduino_GFX_Library.h"
#include "SensorQMI8658.hpp"
#include "TouchDrvCSTXXX.hpp"
#include "config.h"
#include "face.h"
#include "goliash.h"
#include "improv.h"
#include "motion.h"
#include "ota.h"
#include "power.h"
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
Motion motion;
lv_disp_t *disp = nullptr;

const int kBufLines = 40;
lv_disp_draw_buf_t draw_buf;

volatile bool touch_irq = false;
bool touch_ok = false, imu_ok = false, sound_ok = false, power_ok = false;

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
  int16_t x0 = 0, y0 = 0, travel = 0;
  bool petting = false, rotated = false;
} tp;

// Quick taps in a row: 1 = boop, 2 = wink, SETUP_TAPS = setup mode
int taps = 0;
uint32_t last_tap = 0;

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

void wake(uint32_t now) {
  last_activity = now;
  if (asleep) {
    asleep = false;
    setBrightness(true);
    face.setBaseMood(Mood::TIRED);
    face.setMood(Mood::SURPRISED, 700);
  } else if (face.mood() == Mood::DROWSY) {
    face.setBaseMood(Mood::TIRED);
  }
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
      tp.last = now;
      wake(now);
    }
  }
  if (tp.down) {
    const uint32_t held = now - tp.start;
    if (now - tp.last > 120) {  // finger lifted
      tp.down = false;
      face.setPetting(false);
      if (!tp.petting && !tp.rotated && held < 350 && tp.travel < 30) {
        taps = now - last_tap < 500 ? taps + 1 : 1;
        last_tap = now;
        if (taps >= SETUP_TAPS) settings::requestSetup();  // restarts into setup mode
      }
      return;
    }
    if (!tp.rotated && tp.travel < 25 && held > ROTATE_HOLD_MS) {
      tp.rotated = true;
      face.setPetting(false);
      rotateFace();
    } else if (!tp.rotated && (held > 500 || tp.travel > 40)) {
      tp.petting = true;
      face.setPetting(true);
    }
  }
  if (taps && !tp.down && now - last_tap > 500) {  // the tap sequence is over
    if (taps == 2) {
      face.setMood(Mood::WINK, 1300);
      sound::play(Sound::CHIRP);
    } else {  // boop!
      face.setMood(Mood::SURPRISED, 450);
      sound::play(Sound::BOOP);
      happy_after = now + 450;
    }
    face.clearAlert();  // any tap means "seen"
    taps = 0;
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
  if (!goliash::takeAlert(a)) return;
  face.showAlert(a.title, face.unseen() + a.count, a.level);
  if (goliash::quietHours()) return;  // at night: badge only, let him (and you) sleep
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
  pollPower(now);
  improv::loop();
  showStatus(now);
  updateIdle(now);
  face.update(now);
  lv_timer_handler();
  delay(5);
}
