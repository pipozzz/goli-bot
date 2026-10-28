#pragma once

// Goli's voice: little synthesized sounds through the ES8311 codec and the speaker.
// Everything is generated on the fly (no audio files); playback runs in its own task.
#include <stddef.h>
#include <stdint.h>

enum class Sound { DING, ALARM, BOOP, CHIRP, WOBBLE, YAWN };  // ALARM: the bell rung twice, lower

namespace sound {

// Call after Wire.begin(); returns false if the codec does not answer.
bool begin();
void play(Sound s);   // non-blocking; a new sound replaces the one playing
void setVolume(int percent);
void stop();          // hush: cut the sound that is playing
bool playing();       // a sound is playing, or ended less than 300 ms ago (the mics ignore him then)
bool readMic(int16_t *stereo, size_t frames);  // microphone frames (mic 1 / mic 2), blocking

}  // namespace sound
