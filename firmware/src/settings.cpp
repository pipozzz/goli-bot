#include <Preferences.h>

#include "config.h"
#include "settings.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace settings {
namespace {

Settings s;
Preferences prefs;
SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
volatile uint32_t version = 0;  // bumped by every save()

}  // namespace

String randomKey() {
  char key[25];
  for (int i = 0; i < 24; i += 8) snprintf(key + i, 9, "%08lx", (unsigned long)esp_random());
  return key;
}

// The key for the local API, kept apart from the rest so it exists even before setup
void ensureKey() {
  s.api_key = prefs.getString("key");
  if (s.api_key.length()) return;
  s.api_key = randomKey();
  prefs.putString("key", s.api_key);
}

void load() {
  prefs.begin("goli", false);
  ensureKey();
  if (!prefs.isKey("ssid")) {  // never configured on the device: seed from secrets.h (developer builds)
#ifdef WIFI_SSID
    s.ssid = WIFI_SSID;
    s.pass = WIFI_PASS;
#endif
#ifdef GOLIASH_URL
    s.url = GOLIASH_URL;
    s.token = GOLIASH_TOKEN;
    s.ca = GOLIASH_ROOT_CA;
#endif
    return;
  }
  s.ssid = prefs.getString("ssid");
  s.pass = prefs.getString("pass");
  s.url = prefs.getString("url");
  s.token = prefs.getString("token");
  s.ca = prefs.getString("ca");
  s.volume = prefs.getInt("vol", SOUND_VOLUME_DEFAULT);
  s.quiet_from = prefs.getInt("qfrom", 22);
  s.quiet_to = prefs.getInt("qto", 7);
  s.auto_update = prefs.getBool("ota", true);
  s.min_level = prefs.getInt("minlvl", 0);
  s.prod_only = prefs.getBool("prod", false);
  s.services = prefs.getString("svcs");
  s.push = prefs.getBool("push", false);
  s.listen = prefs.getBool("mic", true);
  s.snd_info = prefs.getString("sndi", "ding");
  s.snd_crit = prefs.getString("sndc", "ding-ding");
  s.snd_good = prefs.getString("sndg", "chirp");
  s.talk = prefs.getBool("talk", false);
  s.ics = prefs.getString("ics");
  s.lat = prefs.getString("lat");
  s.lon = prefs.getString("lon");
  s.gh_token = prefs.getString("gh");
  s.watch = prefs.getString("watch");
  s.team_left = prefs.getString("tleft");
  s.team_right = prefs.getString("tright");
  s.buddy = prefs.getBool("buddy", true);
  s.break_min = prefs.getInt("brk", 50);
  s.focus_min = prefs.getInt("focus", FOCUS_MIN);
  s.night_off = prefs.getBool("night", false);
  s.lang = prefs.getString("lang", "en");
  s.mqtt_host = prefs.getString("mqh");
  s.mqtt_user = prefs.getString("mqu");
  s.mqtt_pass = prefs.getString("mqp");
  s.mqtt_port = prefs.getInt("mqport", 1883);
  s.birthday = prefs.getString("bday");
}

void save() {
  xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
  const Settings s = settings::s;  // write a snapshot: NVS commits are slow, don't hold the lock for them
  version = version + 1;
  xSemaphoreGiveRecursive(mutex);
  prefs.putString("ssid", s.ssid);
  prefs.putString("pass", s.pass);
  prefs.putString("url", s.url);
  prefs.putString("token", s.token);
  prefs.putString("ca", s.ca);
  prefs.putInt("vol", s.volume);
  prefs.putInt("qfrom", s.quiet_from);
  prefs.putInt("qto", s.quiet_to);
  prefs.putBool("ota", s.auto_update);
  prefs.putInt("minlvl", s.min_level);
  prefs.putBool("prod", s.prod_only);
  prefs.putString("svcs", s.services);
  prefs.putBool("push", s.push);
  prefs.putBool("mic", s.listen);
  prefs.putString("key", s.api_key);
  prefs.putString("sndi", s.snd_info);
  prefs.putString("sndc", s.snd_crit);
  prefs.putString("sndg", s.snd_good);
  prefs.putBool("talk", s.talk);
  prefs.putString("ics", s.ics);
  prefs.putString("lat", s.lat);
  prefs.putString("lon", s.lon);
  prefs.putString("gh", s.gh_token);
  prefs.putString("watch", s.watch);
  prefs.putString("tleft", s.team_left);
  prefs.putString("tright", s.team_right);
  prefs.putBool("buddy", s.buddy);
  prefs.putInt("brk", s.break_min);
  prefs.putString("bday", s.birthday);
  prefs.putInt("focus", s.focus_min);
  prefs.putBool("night", s.night_off);
  prefs.putString("lang", s.lang);
  prefs.putString("mqh", s.mqtt_host);
  prefs.putString("mqu", s.mqtt_user);
  prefs.putString("mqp", s.mqtt_pass);
  prefs.putInt("mqport", s.mqtt_port);
}

Settings &get() { return s; }
void newKey() { s.api_key = randomKey(); }
bool hasWifi() { return s.ssid.length() > 0; }
bool hasGoliash() { return s.url.length() > 0 && s.token.length() > 0; }

void requestSetup() {
  prefs.putBool("setup", true);
  ESP.restart();
}

bool takeSetupRequest() {
  const bool want = prefs.getBool("setup", false);
  if (want) prefs.remove("setup");
  return want;
}

}  // namespace settings

namespace settings {

void lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGiveRecursive(mutex); }

Settings copy() {
  lock();
  Settings c = s;
  unlock();
  return c;
}

}  // namespace settings

namespace settings {

uint32_t changes() { return version; }

bool tryCopy(Settings &out) {
  if (xSemaphoreTakeRecursive(mutex, 0) != pdTRUE) return false;
  out = s;
  xSemaphoreGiveRecursive(mutex);
  return true;
}

}  // namespace settings
