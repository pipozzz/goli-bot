#pragma once

// Firmware updates over Wi-Fi from the releases published on GitHub Pages (ota.json + app image).
// Only release builds update themselves; the download must come from a server with a trusted
// certificate (src/tls_roots.h) and the image is checked by the ESP32 before it boots.
namespace ota {

// Blocking: call from the network task while Wi-Fi is up. Restarts Goli after a successful update.
// Returns false when the check itself failed (try again sooner).
bool check();
bool updating();  // a download is in progress

}  // namespace ota
