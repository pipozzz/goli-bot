#include "synth.h"

#include <math.h>
#include <string.h>

#include <algorithm>

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

// A ship's bell: lower, longer, a bit rougher
float shipBell(float t) {
  static const Partial partials[] = {
      {0.50f, 0.40f, 2.6f}, {1.00f, 1.00f, 2.0f}, {1.21f, 0.50f, 1.4f}, {1.50f, 0.25f, 1.0f},
      {2.00f, 0.45f, 0.9f}, {2.61f, 0.30f, 0.5f}, {3.10f, 0.15f, 0.35f},
  };
  if (t < 0.f) return 0.f;
  float v = 0.f;
  for (const auto &p : partials) v += p.amp * env(t, 0.003f, p.decay_s) * sinf(2.f * PI * 587.3f * p.ratio * t);
  return v;
}

// A marimba bar: fundamental plus the bar's 4x and 10x partials, quickly gone
float marimba(float t, float f) {
  if (t < 0.f) return 0.f;
  return env(t, 0.002f, 0.32f) * sinf(2.f * PI * f * t) + 0.35f * env(t, 0.001f, 0.06f) * sinf(2.f * PI * 3.93f * f * t) +
         0.12f * env(t, 0.001f, 0.025f) * sinf(2.f * PI * 9.2f * f * t);
}

float square(float phase, float duty) { return fmodf(phase, 1.f) < duty ? 1.f : -1.f; }

// Deterministic noise in -1..1 for sample n (no state between chunks)
float noise(int n) {
  uint32_t x = uint32_t(n) * 2654435761u;
  x ^= x >> 13;
  x *= 0x5bd1e995u;
  x ^= x >> 15;
  return float(x & 0xffff) / 32768.f - 1.f;
}

// ---- Goli's voice: formant synthesis without filter state, so any chunk renders on its own.
// A glottal buzz is a sum of harmonics of the pitch; each harmonic is as loud as the vowel's
// formant resonances are at its frequency.
struct Vowel {
  float f1, f2, f3;
};
const Vowel kA = {730, 1090, 2440}, kE = {530, 1840, 2480}, kI = {300, 2290, 3010}, kO = {500, 850, 2410},
            kU = {320, 870, 2240}, kM = {250, 1100, 2500};

float formantGain(float f, const Vowel &v, bool nasal) {
  auto peak = [](float f, float c, float bw) { const float d = (f - c) / bw; return 1.f / (1.f + d * d); };
  return peak(f, v.f1, 90.f) + (nasal ? 0.08f : 0.55f) * peak(f, v.f2, 120.f) + (nasal ? 0.03f : 0.25f) * peak(f, v.f3, 160.f);
}

// One syllable: vowel a -> b, pitch p0 -> p1 over `dur` seconds, at time t into it
float syllable(float t, float dur, const Vowel &a, const Vowel &b, float p0, float p1, bool nasal) {
  if (t < 0.f || t >= dur) return 0.f;
  const float u = t / dur;
  const Vowel v = {a.f1 + (b.f1 - a.f1) * u, a.f2 + (b.f2 - a.f2) * u, a.f3 + (b.f3 - a.f3) * u};
  const float phase = p0 * t + (p1 - p0) * t * t / (2.f * dur);  // integral of the pitch glide
  const float pitch = p0 + (p1 - p0) * u;
  float s = 0.f;
  for (int k = 1; k * pitch < 3600.f; k++) s += formantGain(k * pitch, v, nasal) / k * sinf(2.f * PI * k * phase);
  const float amp = fminf(1.f, t / 0.03f) * fminf(1.f, (dur - t) / 0.08f);
  return amp * s;
}

}  // namespace

const float kVoiceGain = 0.42f;

bool render(Sound s, int n0, int len, int16_t *out) {
  float dur = 0.f;
  switch (s) {
    case Sound::DING: dur = 1.8f; break;
    case Sound::ALARM: dur = 2.2f; break;
    case Sound::BOOP: dur = 0.16f; break;
    case Sound::CHIRP: dur = 0.32f; break;
    case Sound::WOBBLE: dur = 0.7f; break;
    case Sound::YAWN: dur = 1.1f; break;
    case Sound::SHIP: dur = 3.2f; break;
    case Sound::MARIMBA: dur = 1.0f; break;
    case Sound::CHIP: dur = 0.42f; break;
    case Sound::COIN: dur = 0.5f; break;
    case Sound::DROP: dur = 0.45f; break;
    case Sound::WHISPER: dur = 0.4f; break;
    case Sound::HM: dur = 0.45f; break;
    case Sound::OU: dur = 0.5f; break;
    case Sound::EH: dur = 0.85f; break;
    case Sound::YAY: dur = 0.55f; break;
    case Sound::CUSTOM:
    case Sound::NONE: dur = 0.f; break;
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
      case Sound::SHIP:  // two strikes, like a ship's bell telling the watch
        v = 0.24f * (shipBell(t) + 0.85f * shipBell(t - 0.42f));
        break;
      case Sound::MARIMBA:  // G5, C6, E6
        v = 0.4f * (marimba(t, 784.f) + marimba(t - 0.13f, 1046.5f) + 0.9f * marimba(t - 0.26f, 1318.5f));
        break;
      case Sound::CHIP: {  // 8-bit arpeggio C6 E6 G6 C7
        static const float notes[] = {1046.5f, 1318.5f, 1568.f, 2093.f};
        const int k = std::min(3, int(t / 0.07f));
        const float tt = t - k * 0.07f;
        v = 0.16f * square(notes[k] * t, 0.25f) * (k < 3 ? 1.f : expf(-tt / 0.08f));
        break;
      }
      case Sound::COIN: {  // B5 then E6, square
        const float f = t < 0.08f ? 987.8f : 1318.5f;
        v = 0.16f * square(f * t, 0.5f) * (t < 0.08f ? 1.f : expf(-(t - 0.08f) / 0.12f));
        break;
      }
      case Sound::DROP: {  // a water drop: a quick upward glide, and a smaller one after it
        for (float at : {0.f, 0.19f}) {
          const float tt = t - at;
          if (tt < 0.f) continue;
          const float f = 600.f + 1000.f * (1.f - expf(-tt / 0.03f));
          v += (at ? 0.4f : 0.7f) * expf(-tt / 0.06f) * sinf(2.f * PI * f * tt);
        }
        break;
      }
      case Sound::WHISPER:  // a soft "pff" and a tiny tick, for an office
        v = 0.08f * noise(n0 + i) * sinf(PI * t / dur) + 0.12f * env(t, 0.001f, 0.015f) * sinf(2.f * PI * 2600.f * t);
        break;
      case Sound::HM:  // curious "hm?": a hum that rises like a question
        v = kVoiceGain * syllable(t, dur, kM, kM, 210.f, 330.f, true);
        break;
      case Sound::OU:  // surprised "ou!": high, falling, O to U
        v = kVoiceGain * syllable(t, dur, kO, kU, 420.f, 250.f, false);
        break;
      case Sound::EH:  // tired "ehh...": a sigh with some breath in it
        v = 1.6f * kVoiceGain * syllable(t, dur, kE, kA, 230.f, 150.f, false) +
            0.05f * noise(n0 + i) * sinf(PI * t / dur);
        break;
      case Sound::YAY:  // happy "ya-ay!"
        v = kVoiceGain * (syllable(t, 0.24f, kI, kA, 290.f, 360.f, false) +
                          syllable(t - 0.27f, 0.28f, kA, kI, 340.f, 450.f, false));
        break;
      case Sound::CUSTOM:
      case Sound::NONE:
        break;
    }
    out[i] = int16_t(fmaxf(-1.f, fminf(1.f, v)) * 30000.f);
  }
  return more;
}

}  // namespace synth

const SoundName kSoundNames[] = {
    {Sound::DING, "ding", "Desk bell"},       {Sound::ALARM, "ding-ding", "Desk bell, twice"},
    {Sound::CHIRP, "chirp", "Chirp"},         {Sound::SHIP, "ship", "Ship's bell"},
    {Sound::MARIMBA, "marimba", "Marimba"},   {Sound::CHIP, "chip", "8-bit"},
    {Sound::COIN, "coin", "Coin"},            {Sound::DROP, "drop", "Water drop"},
    {Sound::WHISPER, "whisper", "Whisper"},   {Sound::HM, "hm", "Goli: \"hm?\""},
    {Sound::OU, "ou", "Goli: \"ou!\""},       {Sound::EH, "eh", "Goli: \"ehh...\""},
    {Sound::YAY, "yay", "Goli: \"ya-ay!\""}, {Sound::CUSTOM, "custom", "Your own sound"},
    {Sound::NONE, "none", "Silent"},
};
const int kSoundCount = sizeof kSoundNames / sizeof *kSoundNames;

Sound soundByName(const char *name, Sound fallback) {
  for (int i = 0; name && i < kSoundCount; i++)
    if (!strcmp(name, kSoundNames[i].name)) return kSoundNames[i].sound;
  return fallback;
}
