#include "ota.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>

#include "config.h"
#include "ota_roots.h"
#include "settings.h"

namespace ota {
namespace {

volatile bool busy = false;

}  // namespace

bool check() {
  if (!strcmp(GOLI_VERSION, "dev") || !settings::get().auto_update) return true;

  NetworkClientSecure tls;
  tls.setCACert(kOtaRoots);
  HTTPClient http;
  if (!http.begin(tls, OTA_MANIFEST_URL)) return false;
  http.setTimeout(8000);
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("ota: manifest HTTP %d\n", code);
    http.end();
    return false;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  const char *version = doc["version"] | "";
  const char *path = doc["path"] | "";
  if (err || !*version || !*path) {
    Serial.println("ota: bad manifest");
    return false;
  }
  if (!strcmp(version, GOLI_VERSION)) return true;  // up to date

  // The image sits next to the manifest
  String url = OTA_MANIFEST_URL;
  url = url.substring(0, url.lastIndexOf('/') + 1) + path;
  Serial.printf("ota: %s -> %s from %s\n", GOLI_VERSION, version, url.c_str());
  busy = true;
  vTaskDelay(pdMS_TO_TICKS(1500));  // let the face show "updating" first

  NetworkClientSecure dl;
  dl.setCACert(kOtaRoots);
  httpUpdate.rebootOnUpdate(true);
  const HTTPUpdateResult r = httpUpdate.update(dl, url, GOLI_VERSION);
  busy = false;  // only reached when the update did not happen
  if (r == HTTP_UPDATE_FAILED) {
    Serial.printf("ota: failed (%s)\n", httpUpdate.getLastErrorString().c_str());
    return false;
  }
  return true;
}

bool updating() { return busy; }

}  // namespace ota
