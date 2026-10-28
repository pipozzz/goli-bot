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

void load() {
  prefs.begin("goli", false);
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
}

Settings &get() { return s; }
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
