#pragma once

#include <stdint.h>

#include "sound.h"

// Goli's sounds as plain math, shared by the firmware (I2S) and the browser simulator (Web Audio).
namespace synth {

const int kRate = 16000;  // samples per second, mono

// Fills `out` with samples [n0, n0+len) of the sound; returns false when the sound is over.
bool render(Sound s, int n0, int len, int16_t *out);

}  // namespace synth
