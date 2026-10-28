// Minimal Arduino shim so face.cpp can be rendered on the host (env:snapshot).
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

extern uint32_t g_fake_millis;
inline uint32_t millis() { return g_fake_millis; }
inline long random(long hi) { return hi > 0 ? rand() % hi : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }
template <class T, class L, class H>
inline T constrain(T v, L lo, H hi) { return v < lo ? lo : (v > hi ? hi : v); }
