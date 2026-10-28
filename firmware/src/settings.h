#pragma once

#include <Arduino.h>

#include "config.h"

// Everything a user sets up from the phone, kept in flash (NVS).
// First boot: values from include/secrets.h if it exists, else empty (Goli opens setup mode).
struct Settings {
  String ssid, pass;
  String url, token;  // Goliash
  String ca;          // optional PEM; empty = don't check the certificate
  int volume = SOUND_VOLUME_DEFAULT;
  int quiet_from = 22, quiet_to = 7;
  bool auto_update = true;  // install new releases over Wi-Fi
};

namespace settings {

void load();
void save();
Settings &get();
bool hasWifi();
bool hasGoliash();

void requestSetup();   // reboot into setup mode
bool takeSetupRequest();  // true once after such a reboot

}  // namespace settings
