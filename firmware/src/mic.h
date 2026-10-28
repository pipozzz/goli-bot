#pragma once

// Goli's ears: the board's two microphones through the ES7210 ADC. Only loudness is measured,
// on Goli; no audio is stored or sent anywhere. Ignored while Goli himself makes a sound.
namespace mic {

bool begin();    // after sound::begin() (the mics share its I2S port); false if the ADC does not answer
bool takeClap(); // a sharp clap, reported once
bool loud();     // sustained noise right now
int levelDb();   // loudness of the last ~100 ms, dBFS (-90 .. 0)

}  // namespace mic
