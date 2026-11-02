#pragma once

#include <stddef.h>
#include <stdint.h>

// Goli's ears: the board's two microphones through the ES7210 ADC. Normally only loudness is
// measured, on Goli; audio is kept only while you hold him to ask something (record(), for chat.cpp).
// Ignored while Goli himself makes a sound.
namespace mic {

bool begin();    // after sound::begin() (the mics share its I2S port); false if the ADC does not answer
bool takeClap(); // a sharp clap, reported once
bool loud();     // sustained noise right now
int levelDb();   // loudness of the last ~100 ms, dBFS (-90 .. 0)
uint32_t lastSoundMs();
// Keep what the mics hear as 16 kHz mono in `buf` (up to `max` samples) until stopRecording(),
// which returns how many samples were kept
void record(int16_t *buf, size_t max);
size_t stopRecording();  // millis() when something above the room's quiet level was heard (voices, typing)

}  // namespace mic
