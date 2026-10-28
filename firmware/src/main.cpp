// Goli desk bot: Goli's animated face on the Waveshare ESP32-S3-Touch-AMOLED-1.75.
//
// Touch: tap = boop (surprised, then happy), double tap = wink, finger on the face = petting,
//        hold still for ROTATE_HOLD_MS = turn the face 90° (saved).
// IMU:   tilt = pupils follow, shake = dizzy, bump = surprised, lying flat = falls asleep.
// Idle:  tired -> drowsy -> asleep (dim); any touch or motion wakes him up.

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "Arduino_GFX_Library.h"
#include "SensorQMI8658.hpp"
#include "TouchDrvCSTXXX.hpp"
#include "config.h"
#include "face.h"
#include "goliash.h"
#include "motion.h"
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
  uint32_t pending_tap = 0;   // time of a single tap waiting to see if a second one follows
} tp;

// ---- mood sequencing
uint32_t last_activity = 0;
uint32_t happy_after = 0;     // boop: surprised first, then happy
bool asleep = false;

void setBrightness(bool awake) {
  static int current = -1;
  const int want = awake ? BRIGHTNESS_AWAKE : BRIGHTNESS_ASLEEP;
  if (want != current) gfx->setBrightness(current = want);
}

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
        tp.pending_tap = 0;
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
        if (tp.pending_tap && now - tp.pending_tap < 450) {
          tp.pending_tap = 0;
          face.setMood(Mood::WINK, 1300);
          sound::play(Sound::CHIRP);
        } else {
          tp.pending_tap = now;
        }
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
  if (tp.pending_tap && now - tp.pending_tap > 450) {  // it was a single tap: boop!
    tp.pending_tap = 0;
    face.setMood(Mood::SURPRISED, 450);
    sound::play(Sound::BOOP);
    face.clearAlert();  // a boop means "seen"
    happy_after = now + 450;
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
  face.showAlert(a.title, face.unseen() + a.count);
  if (goliash::quietHours()) return;  // at night: badge only, let him (and you) sleep
  wake(now);
  face.setMood(Mood::SURPRISED, 1200);  // "furt dačo!"
  sound::play(Sound::DING);
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

  if (!asleep && (idle > ASLEEP_AFTER_MS || (flat_since && now - flat_since > 3000))) {
    asleep = true;
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

void setup() {
  Serial.begin(115200);
  Wire.begin(IIC_SDA, IIC_SCL);
  randomSeed(esp_random());
  prefs.begin("goli", false);

  if (!gfx->begin()) Serial.println("display init failed");
  gfx->fillScreen(0x0000);
  setBrightness(true);

  touch.setPins(TP_RESET, TP_INT);
  if (touch.begin(Wire, CST92XX_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    touch.setMaxCoordinates(LCD_SIZE, LCD_SIZE);
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TP_INT), [] { touch_irq = true; }, FALLING);
  } else {
    Serial.println("touch init failed");
  }

  if (imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  } else {
    Serial.println("IMU init failed");
  }

  if (!sound::begin()) Serial.println("audio init failed");

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

  face.begin(lv_scr_act());
  goliash::begin();
  if (!goliash::configured()) Serial.println("no include/secrets.h: Goliash alerts are off");
  last_activity = millis();
  Serial.println("Goli is up. Furt dačo!");
}

void loop() {
  static uint32_t last_tick = millis();
  const uint32_t now = millis();
  lv_tick_inc(now - last_tick);
  last_tick = now;

  pollTouch(now);
  pollImu(now);
  pollGoliash(now);
  updateIdle(now);
  face.update(now);
  lv_timer_handler();
  delay(5);
}
