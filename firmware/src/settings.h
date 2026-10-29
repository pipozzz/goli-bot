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

  // Sounds: names from kSoundNames, per alert level
  String snd_info = "ding", snd_crit = "ding-ding", snd_good = "chirp";
  bool talk = false;        // Goli's own voice for boops and cheers

  // Sources besides Goliash (all optional)
  String ics;               // calendar: a secret iCal address
  String lat, lon;          // weather (Open-Meteo), empty = off
  String gh_token;          // GitHub: review requests, failed CI on your PRs
  String watch;             // URLs to keep an eye on, one per line
  String team_left, team_right;  // hostnames of the Golis next to this one

  bool buddy = true;        // good morning, stretch breaks, Friday
  int break_min = 50;       // minutes at the desk before a stretch break (0 = never)
  int focus_min = FOCUS_MIN;  // focus mode (3 taps)
  bool night_off = false;   // screen off while he sleeps in quiet hours

  // Home Assistant (MQTT discovery); empty host = off
  String mqtt_host, mqtt_user, mqtt_pass;
  int mqtt_port = 1883;
  String birthday;          // "MM-DD"
};

namespace settings {

void load();
void save();
Settings &get();
bool hasWifi();
bool hasGoliash();
void newKey();   // replaces the API key (call save() after)

// Settings change from the web page (its own task) while the network task reads them: change
// them under lock(), and let other tasks read through copy().
void lock();
void unlock();
Settings copy();
bool tryCopy(Settings &out);  // without waiting (the main loop): false when someone holds the lock
uint32_t changes();           // grows with every save()

void requestSetup();   // reboot into setup mode
bool takeSetupRequest();  // true once after such a reboot

}  // namespace settings
