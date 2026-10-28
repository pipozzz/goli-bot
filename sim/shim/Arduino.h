// Just enough Arduino for face.cpp and demo.cpp in the browser simulator.
#pragma once
#include <emscripten.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

inline uint32_t millis() { return uint32_t(emscripten_get_now()); }
inline long random(long hi) { return hi > 0 ? rand() % hi : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }
template <class T, class L, class H>
inline T constrain(T v, L lo, H hi) { return v < lo ? lo : (v > hi ? hi : v); }
