#pragma once

#include <Arduino.h>

// Improv Wi-Fi over the USB serial port (https://www.improv-wifi.com/serial/): right after the
// web installer flashes Goli, it offers to send him your Wi-Fi without the hotspot dance.
namespace improv {

// Where to continue after Wi-Fi works (e.g. Goli's setup page on the LAN); may be empty.
using UrlFn = String (*)();
// Called once Goli joined the network with new credentials (already saved in settings).
using DoneFn = void (*)();

void begin(UrlFn url, DoneFn done);
void loop();  // call often from the main loop; reads Serial

}  // namespace improv
