#pragma once

#include <stdint.h>

// Firmware updates over Wi-Fi from the releases published on GitHub Pages (ota.json + app image).
// Only release builds update themselves; the download must come from a server with a trusted
// certificate (src/tls_roots.h) and the image is checked by the ESP32 before it boots.
namespace ota {

// Blocking: call from the network task while Wi-Fi is up. Restarts Goli after a successful update.
// Returns false when the check itself failed (try again sooner).
bool check();
bool updating();  // a download is in progress

// From the web page: look for a new version now, or install it (also on a dev build, or with
// "install by himself" off). The network task picks it up within a few seconds.
void request(bool install);
void serveRequest();  // network task: does what was asked, if anything

struct Info {
  const char *latest;  // "" until a check got the manifest
  uint32_t checked_s;  // seconds since that check
  const char *state;   // "idle", "checking", "up to date", "downloading", or what went wrong
};
Info info();

}  // namespace ota
