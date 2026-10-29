#include "ntfy.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "goliash.h"
#include "rules.h"
#include "settings.h"
#include "tls.h"

namespace ntfy {
namespace {

volatile const char *last_status = "off";
String last_id;      // resume after a dropped connection without missing or repeating messages...
String last_id_for;  // ...on this topic only: ntfy replays a whole topic for an id it doesn't know
bool started = false;

void handle(const String &line) {
  JsonDocument event;
  if (deserializeJson(event, line)) return;
  const char *kind = event["event"] | "";
  if (strcmp(kind, "message")) return;  // "open", "keepalive"
  last_id = event["id"] | "";
  JsonDocument inner;
  const rules::Hook h = rules::ntfy(event.as<JsonObjectConst>(), inner);
  if (h.kind == rules::Hook::GOLIASH) goliash::pushMessage(inner.as<JsonObjectConst>());
  else if (h.kind != rules::Hook::INVALID) goliash::pushAlert(h.title, h.level, h.count);
}

// The address of the topic's JSON stream: /json before any query (?auth=...), since= after it
String streamUrl(String url) {
  String query;
  const int q = url.indexOf('?');
  if (q >= 0) {
    query = url.substring(q + 1);
    url = url.substring(0, q);
  }
  while (url.endsWith("/")) url.remove(url.length() - 1);
  url += "/json";
  if (last_id.length()) query += String(query.length() ? "&" : "") + "since=" + last_id;
  return query.length() ? url + "?" + query : url;
}

bool sameSubscription(const Settings &a, const Settings &b) {
  return a.ntfy_url == b.ntfy_url && a.ntfy_token == b.ntfy_token;
}

// One subscription, until the connection drops or the topic or token change
void listen(const Settings &cfg) {
  if (last_id_for != cfg.ntfy_url) {  // another topic: start from now
    last_id = "";
    last_id_for = cfg.ntfy_url;
  }
  const String url = streamUrl(cfg.ntfy_url);
  NetworkClientSecure tls;
  trustPublicRoots(tls);
  NetworkClient plain;
  HTTPClient http;
  const bool https = url.startsWith("https://");
  if (!(https ? http.begin(tls, url) : http.begin(plain, url))) {
    last_status = "bad address";
    return;
  }
  http.useHTTP10(true);  // a plain stream of JSON lines, no chunks
  http.setTimeout(10000);
  if (cfg.ntfy_token.length()) http.addHeader("Authorization", "Bearer " + cfg.ntfy_token);
  const int code = http.GET();
  if (code != 200) {
    last_status = code == 401 || code == 403 ? "token refused" : "unreachable";
    Serial.printf("ntfy: HTTP %d\n", code);
    http.end();
    return;
  }
  NetworkClient *stream = http.getStreamPtr();
  if (!stream) {  // closed right after the headers
    last_status = "unreachable";
    http.end();
    return;
  }
  last_status = "listening";
  uint32_t heard = millis(), version = settings::changes();
  String line;
  line.reserve(512);
  while (stream->connected()) {
    if (settings::changes() != version) {  // something changed: only the topic or the token matter here
      version = settings::changes();
      if (!sameSubscription(cfg, settings::copy())) break;
    }
    if (!stream->available()) {
      if (millis() - heard > 120000) break;  // ntfy sends a keepalive every 45 s: this one is dead
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    heard = millis();
    // Only what has arrived, byte by byte: a line that stalls halfway waits for the next round
    // (Stream::readStringUntil would spin for its whole timeout and trip the watchdog)
    while (stream->available()) {
      const int c = stream->read();
      if (c < 0) break;
      if (c == '\n') {
        if (line.length()) handle(line);
        line = "";
      } else if (line.length() < 8192) {
        line += char(c);
      }
    }
  }
  http.end();
}

void task(void *) {
  uint32_t backoff = 5000;
  for (;;) {
    const Settings cfg = settings::copy();
    if (!cfg.ntfy_url.length()) {
      last_status = "off";
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      last_status = "waiting for Wi-Fi";
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    last_status = "connecting";
    const uint32_t began = millis();
    listen(cfg);
    // A connection that lasted a while was fine: reconnect at once; quick failures back off
    backoff = millis() - began > 60000 ? 2000 : std::min<uint32_t>(backoff * 2, 300000);
    if (!sameSubscription(cfg, settings::copy())) backoff = 0;
    vTaskDelay(pdMS_TO_TICKS(backoff));
  }
}

}  // namespace

void begin() {  // only once a topic is set up: a TLS connection costs ~40 KB of internal RAM
  if (started) return;
  started = true;
  xTaskCreatePinnedToCore(task, "goli_ntfy", 10240, nullptr, 1, nullptr, 0);
}

const char *status() { return (const char *)last_status; }

}  // namespace ntfy
