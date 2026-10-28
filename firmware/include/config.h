#pragma once

// Release tag, set by tools/version.py (CI); local builds are "dev" and never self-update.
#ifndef GOLI_VERSION
#define GOLI_VERSION "dev"
#endif

// ---- Waveshare ESP32-S3-Touch-AMOLED-1.75 pins (from Waveshare's pin_config.h)
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_SIZE 466

#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

// ES8311 codec (I2C 0x18) + speaker amplifier enable
#define I2S_MCLK 42
#define I2S_BCLK 9
#define I2S_WS 45
#define I2S_DOUT 8   // ESP32 -> codec (speaker)
#define I2S_DIN 10   // codec -> ESP32 (microphone, unused)
#define AUDIO_PA_EN 46

// ---- Behaviour
// Speaker volume 0-100 before the user sets one in setup (0 = silent Goli).
#define SOUND_VOLUME_DEFAULT 70

// Goliash notifications. Wi-Fi, Goliash address, token, volume and quiet hours are set up
// from a phone (setup mode); include/secrets.h can pre-fill them for developer builds.
#define GOLIASH_POLL_S 30
#define GOLI_TZ "CET-1CEST,M3.5.0,M10.5.0/3"   // POSIX TZ for Bratislava / Prague / Vienna

// Screen brightness (0-255). AMOLED: keep it moderate on a desk, it also limits burn-in.
#define BRIGHTNESS_AWAKE 170
#define BRIGHTNESS_ASLEEP 25

// Idle -> drowsy -> asleep (ms without touch or motion).
#define DROWSY_AFTER_MS (60UL * 1000)
#define ASLEEP_AFTER_MS (5UL * 60 * 1000)

// Firmware updates over Wi-Fi: every release is published on GitHub Pages. Goli checks a few
// minutes after starting and then once a day (can be turned off on the setup page).
#define OTA_MANIFEST_URL "https://pipozzz.github.io/goli-bot/ota.json"
#define OTA_FIRST_CHECK_S 180
#define OTA_EVERY_H 24

// On battery: show a low-battery icon below this level, and switch the screen off when he
// has been asleep this long (any touch or motion turns it back on).
#define BATTERY_LOW_PCT 20
#define SCREEN_OFF_AFTER_MS (30UL * 60 * 1000)

// Finger on the face while Goli starts (keep it there ~1 s) = self-test screen: touch, tilt,
// sound, Wi-Fi and battery, for checking a freshly assembled Goli.
#define SELFTEST_HOLD_MS 1000

// Hold a finger still on the face this long to turn the face by 90° (saved in flash).
// Use it once after assembly if Goli looks sideways.
#define ROTATE_HOLD_MS 3000

// Tap the face this many times quickly to open setup mode again.
#define SETUP_TAPS 5
// Setup mode gives up after this long when Goli already has a working configuration.
#define SETUP_TIMEOUT_MS (10UL * 60 * 1000)

// The IMU sits on the board; if the pupils roll the wrong way when you tilt Goli,
// flip these signs.
#define TILT_ROLL_SIGN 1.0f
#define TILT_PITCH_SIGN 1.0f
