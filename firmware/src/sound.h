#pragma once

// Goli's voice: little synthesized sounds through the ES8311 codec and the speaker.
// Everything is generated on the fly (no audio files); playback runs in its own task.
enum class Sound { DING, BOOP, CHIRP, WOBBLE, YAWN };

namespace sound {

// Call after Wire.begin(); returns false if the codec does not answer.
bool begin();
void play(Sound s);   // non-blocking; a new sound replaces the one playing
void setVolume(int percent);

}  // namespace sound
