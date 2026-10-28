#pragma once

// Setup mode: Goli opens a WPA2 hotspot, shows a QR code to join it, and serves a setup page
// (captive portal, so phones open it by themselves). Never returns: restarts after saving,
// or after SETUP_TIMEOUT_MS when there is already a working configuration.
namespace setup_mode {

void run();

}  // namespace setup_mode
