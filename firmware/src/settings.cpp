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
}

void save() {
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
