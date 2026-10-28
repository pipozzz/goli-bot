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

  // Which Goliash alerts ring: all, or critical only; only production; only these services
  // (comma-separated, empty = all). Items without an environment or service always pass.
  int min_level = 0;        // AlertLevel as int: 0 = everything, 2 = critical only
  bool prod_only = false;
  String services;
  bool push = false;        // Goliash sends webhooks to Goli: stop polling for notifications

  String api_key;           // for POST /notify and Goliash webhook signatures; made on first start
  bool listen = true;       // microphones: claps and noise (audio never leaves Goli)
};

namespace settings {

void load();
void save();
Settings &get();
bool hasWifi();
bool hasGoliash();
void newKey();   // replaces the API key (call save() after)

void requestSetup();   // reboot into setup mode
bool takeSetupRequest();  // true once after such a reboot

}  // namespace settings
