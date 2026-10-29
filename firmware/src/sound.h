#pragma once

// Goli's voice: little synthesized sounds through the ES8311 codec and the speaker.
// Everything is generated on the fly (no audio files); playback runs in its own task.
#include <stddef.h>
#include <stdint.h>

enum class Sound {
  DING, ALARM, BOOP, CHIRP, WOBBLE, YAWN,  // ALARM: the bell rung twice, lower
  SHIP, MARIMBA, CHIP, COIN, DROP, WHISPER,  // more to pick from for alerts
  HM, OU, EH, YAY,                         // Goli's voice: "hm?", "ou!", "ehh...", "ya-ay!"
  CUSTOM,                                  // a sound uploaded from the web page
  NONE,
};

// Sounds you can pick for alerts, by the name stored in settings and shown on the web page
struct SoundName {
  Sound sound;
  const char *name;   // stored in settings
  const char *label;  // shown on the web page
};
extern const SoundName kSoundNames[];
extern const int kSoundCount;
Sound soundByName(const char *name, Sound fallback);

namespace sound {

// Call after Wire.begin(); returns false if the codec does not answer.
bool begin();
void play(Sound s);   // non-blocking; a new sound replaces the one playing
void setVolume(int percent);
void stop();          // hush: cut the sound that is playing
bool playing();       // a sound is playing, or ended less than 300 ms ago (the mics ignore him then)
bool readMic(int16_t *stereo, size_t frames);  // microphone frames (mic 1 / mic 2), blocking
// The custom sound: 16 kHz mono PCM (the web page converts any file), kept in flash
bool setCustom(const int16_t *pcm, size_t samples);
size_t customSamples();

}  // namespace sound
