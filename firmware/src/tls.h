#pragma once

#include <NetworkClientSecure.h>

// HTTPS to public services (calendars, Open-Meteo, GitHub, watched sites): trust the same root
// CAs as a browser, from the Mozilla bundle built into ESP-IDF.
inline void trustPublicRoots(NetworkClientSecure &client) {
  extern const uint8_t bundle_start[] asm("_binary_x509_crt_bundle_start");
  extern const uint8_t bundle_end[] asm("_binary_x509_crt_bundle_end");
  client.setCACertBundle(bundle_start, bundle_end - bundle_start);
}
