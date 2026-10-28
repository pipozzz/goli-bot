#include "mic.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "settings.h"
#include "sound.h"

namespace mic {
namespace {

const uint8_t kAddr = 0x40;  // ES7210
const int kFrames = 256;     // 16 ms at 16 kHz

volatile bool clap = false, noisy = false;
volatile int level = -90;

bool reg(uint8_t r, uint8_t v) {
  Wire.beginTransmission(kAddr);
  Wire.write(r);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

int read(uint8_t r) {
  Wire.beginTransmission(kAddr);
  Wire.write(r);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(kAddr, uint8_t(1)) != 1) return -1;
  return Wire.read();
}

bool update(uint8_t r, uint8_t mask, uint8_t v) {
  const int cur = read(r);
  return cur >= 0 && reg(r, uint8_t((cur & ~mask) | (v & mask)));
}

// ES7210 as an I2S slave, 16-bit, mics 1 and 2 at +30 dB. The register sequence follows
// Espressif's esp_codec_dev es7210 driver (Apache-2.0): open, mic select, format, start.
bool initAdc() {
  bool ok = reg(0x00, 0xff) && reg(0x00, 0x41);                  // reset
  ok = ok && reg(0x01, 0x3f) && reg(0x09, 0x30) && reg(0x0a, 0x30);  // clocks off, state cycles
  ok = ok && reg(0x23, 0x2a) && reg(0x22, 0x0a) && reg(0x20, 0x0a) && reg(0x21, 0x2a);  // HPF
  ok = ok && update(0x08, 0x01, 0x00);                          // slave
  ok = ok && reg(0x40, 0x43) && reg(0x41, 0x70) && reg(0x42, 0x70);  // analog power, mic bias 2.87 V
  ok = ok && reg(0x07, 0x20) && reg(0x02, 0xc1);                // OSR, main clock
  for (uint8_t r = 0x43; r <= 0x46; r++) ok = ok && update(r, 0x10, 0x00);
  ok = ok && reg(0x4b, 0xff) && reg(0x4c, 0xff);
  ok = ok && update(0x01, 0x0b, 0x00) && reg(0x4b, 0x00);       // mic 1 + 2 on
  ok = ok && update(0x43, 0x1f, 0x10 | 0x0a) && update(0x44, 0x1f, 0x10 | 0x0a);  // PGA on, 30 dB
  ok = ok && reg(0x12, 0x00);                                   // plain I2S, not TDM
  ok = ok && update(0x11, 0x03, 0x00) && update(0x11, 0xe0, 0x60);  // I2S format, 16 bit
  const int off = read(0x01);
  ok = ok && off >= 0;
  ok = ok && reg(0x01, uint8_t(off)) && reg(0x06, 0x00) && reg(0x40, 0x43);  // start
  for (uint8_t r = 0x47; r <= 0x4a; r++) ok = ok && reg(r, 0x08);
  ok = ok && reg(0x40, 0x43) && reg(0x00, 0x71) && reg(0x00, 0x41);
  return ok;
}

void task(void *) {
  static int16_t buf[kFrames * 2];
  float floor_db = -60.f, avg_db = -90.f;
  uint32_t onset = 0, loud_since = 0, calm_since = 0;
  for (;;) {
    if (!sound::readMic(buf, kFrames)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (!settings::get().listen || sound::playing()) {  // don't hear himself
      onset = loud_since = 0;
      continue;
    }
    double sum = 0;
    for (int i = 0; i < kFrames * 2; i++) sum += double(buf[i]) * buf[i];
    const float rms = sqrtf(float(sum / (kFrames * 2)));
    const float db = rms > 1.f ? 20.f * log10f(rms / 32768.f) : -90.f;
    avg_db = avg_db * 0.84f + db * 0.16f;  // ~100 ms
    level = int(avg_db);
    // The room's quiet level: follows quiet moments fast and a louder room slowly (about a minute)
    floor_db += (db - floor_db) * (db < floor_db ? 0.05f : 0.0003f);

    const uint32_t now = millis();
    // A clap: a sharp jump well above the room that is gone again within 200 ms
    if (!onset && db > floor_db + 25.f && db > -32.f) onset = now;
    if (onset && db < floor_db + 12.f) {
      if (now - onset < 200) clap = true;
      onset = 0;
    } else if (onset && now - onset > 200) {
      onset = 0;  // too long for a clap
    }
    // Noise: clearly above the room for 1.5 s; over after 1 s of calm
    if (db > floor_db + 15.f && db > -40.f) {
      if (!loud_since) loud_since = now;
      calm_since = 0;
      if (now - loud_since > 1500) noisy = true;
    } else {
      if (!calm_since) calm_since = now;
      if (now - calm_since > 1000) noisy = false, loud_since = 0;
    }
  }
}

}  // namespace

bool begin() {
  if (!initAdc()) return false;
  xTaskCreatePinnedToCore(task, "goli_mic", 4096, nullptr, 1, nullptr, 0);
  return true;
}

bool takeClap() {
  const bool c = clap;
  clap = false;
  return c;
}

bool loud() { return noisy; }

int levelDb() { return level; }

}  // namespace mic
