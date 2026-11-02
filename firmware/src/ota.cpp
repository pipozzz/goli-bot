#include "ota.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>

#include "config.h"
#include "tls_roots.h"
#include "settings.h"

namespace ota {
namespace {

volatile bool busy = false;
enum Want : uint8_t { NOTHING, CHECK, INSTALL };
volatile uint8_t wanted = NOTHING;
char latest[24] = "";
char path[64] = "";
volatile uint32_t checked_at = 0;  // millis(); 0 = never
const char *volatile state = "idle";

// The manifest into `latest` and `path`; false when it could not be read
bool fetch() {
  state = "checking";
  NetworkClientSecure tls;
  tls.setCACert(kTlsRoots);
  HTTPClient http;
  if (!http.begin(tls, OTA_MANIFEST_URL)) {
    state = "can't reach GitHub";
    return false;
  }
  http.setTimeout(8000);
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("ota: manifest HTTP %d\n", code);
    http.end();
    state = "can't reach GitHub";
    return false;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  const char *v = doc["version"] | "";
  const char *p = doc["path"] | "";
  if (err || !*v || !*p) {
    Serial.println("ota: bad manifest");
    state = "bad manifest";
    return false;
  }
  snprintf(latest, sizeof latest, "%s", v);
  snprintf(path, sizeof path, "%s", p);
  checked_at = millis() | 1;
  state = strcmp(latest, GOLI_VERSION) ? "new version" : "up to date";
  return true;
}

bool install() {
  // The image sits next to the manifest
  String url = OTA_MANIFEST_URL;
  url = url.substring(0, url.lastIndexOf('/') + 1) + path;
  Serial.printf("ota: %s -> %s from %s\n", GOLI_VERSION, latest, url.c_str());
  state = "downloading";
  busy = true;
  vTaskDelay(pdMS_TO_TICKS(1500));  // let the face show "updating" first

  NetworkClientSecure dl;
  dl.setCACert(kTlsRoots);
  httpUpdate.rebootOnUpdate(true);
  const HTTPUpdateResult r = httpUpdate.update(dl, url, GOLI_VERSION);
  busy = false;  // only reached when the update did not happen
  if (r == HTTP_UPDATE_FAILED) {
    Serial.printf("ota: failed (%s)\n", httpUpdate.getLastErrorString().c_str());
    state = "download failed";
    return false;
  }
  return true;
}

}  // namespace

bool check() {
  if (!strcmp(GOLI_VERSION, "dev") || !settings::get().auto_update) return true;
  if (!fetch()) return false;
  if (!strcmp(latest, GOLI_VERSION)) return true;  // up to date
  return install();
}

void request(bool install) { wanted = install ? INSTALL : CHECK; }

void serveRequest() {
  const uint8_t w = wanted;
  if (w == NOTHING) return;
  wanted = NOTHING;
  if (fetch() && w == INSTALL && strcmp(latest, GOLI_VERSION)) install();
}

bool updating() { return busy; }

Info info() {
  return {latest, checked_at ? (millis() - checked_at) / 1000 : 0, state};
}

}  // namespace ota
