#include "sound.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <LittleFS.h>
#include <math.h>

#include <algorithm>

#include "config.h"
#include "es8311.h"
#include "settings.h"
#include "synth.h"

namespace sound {
namespace {

const int kRate = synth::kRate;
const int kChunk = 256;  // samples per I2S write

I2SClass i2s;
es8311_handle_t codec = nullptr;
QueueHandle_t queue = nullptr;
int volume = 0;
volatile bool stopping = false;
volatile uint32_t quiet_since = 0;  // millis() when the last sound ended; 0 = playing

// The custom sound, in PSRAM; swapped only while nothing plays
const char *kCustomPath = "/custom.pcm";
const size_t kCustomMax = synth::kRate * 3;
int16_t *custom = nullptr;
volatile size_t custom_n = 0;

bool loadCustom() {
  File f = LittleFS.open(kCustomPath, "r");
  if (!f) return false;
  const size_t n = std::min(kCustomMax, size_t(f.size()) / 2);
  if (!custom) custom = (int16_t *)ps_malloc(kCustomMax * sizeof(int16_t));
  if (!custom) return false;
  custom_n = f.read((uint8_t *)custom, n * 2) / 2;
  return custom_n > 0;
}

// Next chunk of the custom sound
bool renderCustom(int pos, int len, int16_t *out) {
  for (int i = 0; i < len; i++) out[i] = size_t(pos + i) < custom_n ? custom[pos + i] : 0;
  return size_t(pos + len) < custom_n;
}

void task(void *) {
  int16_t mono[kChunk];
  int16_t stereo[kChunk * 2];
  Sound cur = Sound::DING;
  int pos = -1;  // -1: idle
  for (;;) {
    Sound next;
    if (xQueueReceive(queue, &next, pos < 0 ? portMAX_DELAY : 0) == pdTRUE) {
      cur = next;
      pos = 0;
      quiet_since = 0;
      stopping = false;
      digitalWrite(AUDIO_PA_EN, HIGH);  // speaker amplifier on only while playing (no hiss)
    }
    if (pos < 0) continue;
    const bool more = !stopping && (cur == Sound::CUSTOM ? renderCustom(pos, kChunk, mono)
                                                         : synth::render(cur, pos, kChunk, mono));
    if (stopping) memset(mono, 0, sizeof mono);
    for (int i = 0; i < kChunk; i++) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
    i2s.write((uint8_t *)stereo, sizeof stereo);
    pos += kChunk;
    if (!more) {
      pos = -1;
      memset(stereo, 0, sizeof stereo);
      for (int k = 0; k < 4; k++) i2s.write((uint8_t *)stereo, sizeof stereo);  // flush the DMA tail
      digitalWrite(AUDIO_PA_EN, LOW);
      stopping = false;
      quiet_since = millis() | 1;
    }
  }
}

}  // namespace

bool begin() {
  pinMode(AUDIO_PA_EN, OUTPUT);
  digitalWrite(AUDIO_PA_EN, LOW);

  if (LittleFS.begin(true)) loadCustom();  // the "spiffs" partition of default_16MB.csv

  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return false;

  // The codec shares the I2C bus with touch and IMU: configure it here, from the main task only.
  codec = es8311_create(0, ES8311_ADDRRES_0);
  if (!codec) return false;
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = kRate * 256,
      .sample_frequency = kRate,
  };
  if (es8311_init(codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  es8311_sample_frequency_config(codec, clk.mclk_frequency, clk.sample_frequency);
  es8311_microphone_config(codec, false);
  setVolume(settings::get().volume);

  quiet_since = 1;
  queue = xQueueCreate(4, sizeof(Sound));
  xTaskCreatePinnedToCore(task, "goli_sound", 4096, nullptr, 2, nullptr, 0);
  return true;
}

void play(Sound s) {
  if (!queue || volume == 0 || s == Sound::NONE || (s == Sound::CUSTOM && !custom_n)) return;
  xQueueReset(queue);
  xQueueSend(queue, &s, 0);
}

void stop() { stopping = true; }

bool setCustom(const int16_t *pcm, size_t samples) {
  samples = std::min(samples, kCustomMax);
  File f = LittleFS.open(kCustomPath, "w");
  if (!f) return false;
  const bool ok = f.write((const uint8_t *)pcm, samples * 2) == samples * 2;
  f.close();
  stop();
  for (int i = 0; i < 40 && quiet_since == 0; i++) vTaskDelay(pdMS_TO_TICKS(10));  // let the task let go of it
  return ok && loadCustom();
}

size_t customSamples() { return custom_n; }

bool playing() { return quiet_since == 0 || millis() - quiet_since < 300; }

bool readMic(int16_t *stereo, size_t frames) {
  if (!queue) return false;
  const size_t want = frames * 2 * sizeof(int16_t);
  return i2s.readBytes((char *)stereo, want) == want;
}

void setVolume(int percent) {
  volume = percent;
  if (codec) es8311_voice_volume_set(codec, percent, nullptr);
}

}  // namespace sound
