#include "sound.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>

#include "config.h"
#include "es8311.h"
#include "settings.h"

namespace sound {
namespace {

const int kRate = 16000;
const int kChunk = 256;  // samples per I2S write

I2SClass i2s;
es8311_handle_t codec = nullptr;
QueueHandle_t queue = nullptr;
int volume = 0;
volatile bool stopping = false;
volatile uint32_t quiet_since = 0;  // millis() when the last sound ended; 0 = playing

// One voice: a sum of decaying partials, optionally with a pitch glide.
struct Partial {
  float ratio, amp, decay_s;
};

float env(float t, float attack_s, float decay_s) {
  return (t < attack_s ? t / attack_s : 1.f) * expf(-t / decay_s);
}

// The desk bell struck at t = 0: inharmonic partials (hum, prime, tierce, quint, nominal...)
float bell(float t, float f0) {
  static const Partial partials[] = {
      {0.50f, 0.35f, 1.6f}, {1.00f, 1.00f, 1.1f}, {1.19f, 0.45f, 0.8f}, {1.50f, 0.30f, 0.6f},
      {2.00f, 0.55f, 0.55f}, {2.52f, 0.25f, 0.35f}, {2.74f, 0.20f, 0.3f}, {3.76f, 0.12f, 0.2f},
  };
  if (t < 0.f) return 0.f;
  float v = 0.f;
  for (const auto &p : partials) v += p.amp * env(t, 0.002f, p.decay_s) * sinf(2.f * PI * f0 * p.ratio * t);
  return v;
}

// Fills `out` with samples [n0, n0+len) of the sound; returns false when the sound is over.
bool render(Sound s, int n0, int len, int16_t *out) {
  float dur = 0.f;
  switch (s) {
    case Sound::DING: dur = 1.8f; break;
    case Sound::ALARM: dur = 2.2f; break;
    case Sound::BOOP: dur = 0.16f; break;
    case Sound::CHIRP: dur = 0.32f; break;
    case Sound::WOBBLE: dur = 0.7f; break;
    case Sound::YAWN: dur = 1.1f; break;
  }
  bool more = true;
  for (int i = 0; i < len; i++) {
    const float t = float(n0 + i) / kRate;
    if (t >= dur) {
      out[i] = 0;
      more = false;
      continue;
    }
    float v = 0.f;
    switch (s) {
      case Sound::DING:
        v = 0.32f * bell(t, 1318.5f);  // E6
        break;
      case Sound::ALARM:  // ding-ding, a tone lower: something is on fire
        v = 0.26f * (bell(t, 1174.7f) + 0.9f * bell(t - 0.3f, 1174.7f));  // D6
        break;
      case Sound::BOOP: {  // quick rising "bup"
        const float f = 420.f + 900.f * t / dur;
        v = 0.8f * env(t, 0.004f, 0.06f) * sinf(2.f * PI * f * t);
        break;
      }
      case Sound::CHIRP: {  // two happy notes
        const float f = t < 0.14f ? 1046.5f : 1568.f;
        const float tt = t < 0.14f ? t : t - 0.14f;
        v = 0.55f * env(tt, 0.004f, 0.07f) * sinf(2.f * PI * f * t);
        break;
      }
      case Sound::WOBBLE: {  // dizzy warble sliding down
        const float f = 700.f - 300.f * t / dur + 90.f * sinf(2.f * PI * 11.f * t);
        v = 0.45f * (1.f - t / dur) * sinf(2.f * PI * f * t);
        break;
      }
      case Sound::YAWN: {  // low, slow glide down with a soft attack
        const float f = 330.f - 140.f * t / dur;
        v = 0.5f * sinf(PI * t / dur) * (sinf(2.f * PI * f * t) + 0.3f * sinf(4.f * PI * f * t));
        break;
      }
    }
    out[i] = int16_t(fmaxf(-1.f, fminf(1.f, v)) * 30000.f);
  }
  return more;
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
    const bool more = !stopping && render(cur, pos, kChunk, mono);
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
  if (!queue || volume == 0) return;
  xQueueReset(queue);
  xQueueSend(queue, &s, 0);
}

void stop() { stopping = true; }

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
