#include "synth.h"

#include <math.h>

namespace synth {
namespace {

const float PI = 3.14159265f;

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
}  // namespace

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

}  // namespace synth
