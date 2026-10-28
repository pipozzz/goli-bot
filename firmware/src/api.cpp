#include "api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <mbedtls/md.h>
#include <time.h>

#include "config.h"
#include "goliash.h"
#include "settings.h"

namespace api {
namespace {

WebServer server(80);
bool running = false;
char host[16] = "";

// Constant-time compare, so the key can't be guessed byte by byte from response times
bool same(const String &a, const String &b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); i++) diff |= uint8_t(a[i] ^ b[i]);
  return diff == 0;
}

// Goliash webhook: X-Goliash-Signature: sha256=hex(HMAC-SHA256(secret, timestamp + "." + body))
bool signedByGoliash(const String &body) {
  const String sig = server.header("X-Goliash-Signature");
  const String ts = server.header("X-Goliash-Timestamp");
  if (!sig.startsWith("sha256=") || !ts.length()) return false;
  const time_t now = time(nullptr);
  if (now > 1700000000 && llabs(int64_t(now) - ts.toInt()) > 300) return false;  // stale or replayed
  const String key = settings::get().api_key;
  uint8_t mac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const uint8_t *)key.c_str(), key.length());
  mbedtls_md_hmac_update(&ctx, (const uint8_t *)ts.c_str(), ts.length());
  mbedtls_md_hmac_update(&ctx, (const uint8_t *)".", 1);
  mbedtls_md_hmac_update(&ctx, (const uint8_t *)body.c_str(), body.length());
  mbedtls_md_hmac_finish(&ctx, mac);
  mbedtls_md_free(&ctx);
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", mac[i]);
  return same(sig.substring(7), String(hex));
}

bool bearer() {
  const String auth = server.header("Authorization");
  return auth.startsWith("Bearer ") && same(auth.substring(7), settings::get().api_key);
}

void reply(int code, const char *msg) {
  JsonDocument doc;
  doc[code < 300 ? "status" : "error"] = msg;
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

AlertLevel parseLevel(const char *s) {
  if (!strcmp(s, "critical")) return AlertLevel::CRITICAL;
  if (!strcmp(s, "good")) return AlertLevel::GOOD;
  return AlertLevel::INFO;
}

void notify() {
  const String body = server.arg("plain");
  const bool goliash_hook = server.hasHeader("X-Goliash-Signature");
  if (!(goliash_hook ? signedByGoliash(body) : bearer())) return reply(401, "wrong key or signature");

  JsonDocument doc;
  if (!body.length()) return reply(415, "send JSON with Content-Type: application/json");
  if (deserializeJson(doc, body)) return reply(400, "send JSON");
  JsonObjectConst msg = doc.as<JsonObjectConst>();
  bool rang;
  if (msg["items"].is<JsonArrayConst>()) {  // a Goliash notifier message
    rang = goliash::pushMessage(msg);
  } else {
    const char *title = msg["title"] | "";
    if (!*title) return reply(400, "title is required");
    rang = goliash::pushAlert(title, parseLevel(msg["level"] | "info"));
  }
  reply(202, rang ? "ringing" : "filtered out");
}

void status() {
  JsonDocument doc;
  doc["name"] = hostname();
  doc["version"] = GOLI_VERSION;
  doc["uptime_s"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  static const char *states[] = {"off", "connecting", "no_wifi", "unreachable", "bad_token", "ok"};
  doc["goliash"] = states[int(goliash::status())];
  doc["mode"] = settings::get().push ? "webhook" : "polling";
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void root() {
  server.send(200, "text/plain; charset=utf-8",
              "Hi, I'm Goli. POST /notify to ring me (see the README), GET /status.\n"
              "To change my settings, tap my face 5 times.\n");
}

// The server gets its own task: a slow client must never stall the face
void task(void *) {
  for (;;) {
    if (!running && WiFi.status() == WL_CONNECTED) {
      server.begin();
      MDNS.begin(hostname());
      MDNS.addService("http", "tcp", 80);
      running = true;
      Serial.printf("api: http://%s.local/ (%s)\n", hostname(), WiFi.localIP().toString().c_str());
    }
    if (running) server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(running ? 2 : 500));
  }
}

}  // namespace

void begin() {
  WiFi.setHostname(hostname());
  static const char *headers[] = {"Authorization", "X-Goliash-Signature", "X-Goliash-Timestamp"};
  server.collectHeaders(headers, 3);
  server.on("/", HTTP_GET, root);
  server.on("/status", HTTP_GET, status);
  server.on("/notify", HTTP_POST, notify);
  xTaskCreatePinnedToCore(task, "goli_api", 8192, nullptr, 1, nullptr, 0);
}

const char *hostname() {
  if (!*host) {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // works before Wi-Fi is started
    snprintf(host, sizeof host, "goli-%02x%02x", mac[4], mac[5]);
  }
  return host;
}

}  // namespace api
