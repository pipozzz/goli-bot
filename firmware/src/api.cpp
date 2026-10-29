#include "api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <mbedtls/md.h>
#include <time.h>

#include <algorithm>

#include <Preferences.h>

#include "calendar.h"
#include "config.h"
#include "github.h"
#include "goliash.h"
#include "settings.h"
#include "sound.h"
#include "team.h"
#include "watch.h"
#include "weather.h"
#include "web_page.h"

namespace api {
namespace {

WebServer server(80);
bool running = false;
char host[16] = "";
QueueHandle_t requests = xQueueCreate(8, sizeof(Request));

// ---- pairing: the page asks, Goli shows a code, whoever reads it gets a session
Preferences store;              // "goli-web": sessions survive restarts
const int kSessions = 6;
volatile int pair_code = 0;
uint32_t pair_until = 0, pair_asked = 0;
int pair_tries = 0;           // wrong guesses for the current code
int pair_failures = 0;        // all wrong guesses since the last pairing: each 5 double the wait
uint32_t pair_locked_until = 0;

void ask(Command cmd, int arg = 0) {
  Request r = {cmd, arg};
  xQueueSend(requests, &r, 0);
}

size_t synthBytes() { return 16000 * 3 * sizeof(int16_t); }  // 3 s at 16 kHz, like sound.cpp keeps

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

// Only answer requests made to Goli by his name or address: a page elsewhere can't reach him
// through DNS rebinding (a hostile name pointed at his IP)
bool ourHost() {
  String host = server.header("Host");
  const int colon = host.indexOf(':');
  if (colon >= 0) host = host.substring(0, colon);
  host.toLowerCase();
  const String name = hostname();
  return !host.length() || host == name || host == name + ".local" || host == WiFi.localIP().toString();
}

void forget() {  // every paired browser
  for (int i = 0; i < kSessions; i++) {
    char k[4];
    snprintf(k, sizeof k, "s%d", i);
    store.remove(k);
  }
}

// A paired browser (or the API key, for scripts)
bool session() {
  if (!ourHost()) return false;
  const String auth = server.header("Authorization");
  if (!auth.startsWith("Bearer ")) return false;
  const String token = auth.substring(7);
  if (token.length() < 16) return false;
  if (same(token, settings::get().api_key)) return true;
  for (int i = 0; i < kSessions; i++) {
    char k[4];
    snprintf(k, sizeof k, "s%d", i);
    if (same(token, store.getString(k))) return true;
  }
  return false;
}

void sendJson(int code, const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

bool readJson(JsonDocument &doc) {
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"send JSON\"}");
    return false;
  }
  return true;
}

#define NEED_SESSION()                                                              \
  if (!session()) {                                                                 \
    server.send(401, "application/json", "{\"error\":\"pair this browser first\"}"); \
    return;                                                                         \
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

void root() { server.send_P(200, "text/html; charset=utf-8", kWebPage); }

void pairStart() {
  const uint32_t now = millis();
  if (!ourHost()) return reply(403, "use Goli's own address");
  if (pair_locked_until && int32_t(pair_locked_until - now) > 0) return reply(429, "too many wrong codes: wait a while");
  if (pair_asked && now - pair_asked < 10000) return reply(429, "wait a few seconds");
  pair_asked = now;
  pair_code = 100000 + int(esp_random() % 900000);
  pair_until = now + 120000;
  pair_tries = 0;
  ask(Command::PAIR_CODE, pair_code);
  reply(200, "look at Goli");
}

void pair() {
  JsonDocument in;
  if (!readJson(in)) return;
  const int code = in["code"] | -1;
  if (!ourHost()) return reply(403, "use Goli's own address");
  if (!pair_code || int32_t(millis() - pair_until) > 0) return reply(410, "ask for a new code");
  if (code != pair_code) {
    pair_failures++;
    if (++pair_tries >= 5) {  // this code is gone; and the wait for a new one doubles each time
      pair_code = 0;
      ask(Command::PAIR_CODE, 0);
      pair_locked_until = millis() + (60000UL << std::min(pair_failures / 5 - 1, 10));
    }
    return reply(403, "wrong code");
  }
  pair_code = 0;
  pair_failures = 0;
  pair_locked_until = 0;
  ask(Command::PAIR_CODE, 0);
  char token[33];
  for (int i = 0; i < 32; i += 8) snprintf(token + i, 9, "%08lx", (unsigned long)esp_random());
  const int slot = store.getInt("next", 0);  // replaces the oldest session
  char k[4];
  snprintf(k, sizeof k, "s%d", slot);
  store.putString(k, token);
  store.putInt("next", (slot + 1) % kSessions);
  JsonDocument out;
  out["token"] = token;
  sendJson(200, out);
}

void getConfig() {
  NEED_SESSION();
  const Settings c = settings::copy();
  JsonDocument d;
  d["host"] = hostname();
  d["version"] = GOLI_VERSION;
  d["ssid"] = c.ssid;
  d["has_pass"] = c.pass.length() > 0;
  d["url"] = c.url;
  d["has_token"] = c.token.length() > 0;
  d["push"] = c.push;
  d["minlvl"] = c.min_level;
  d["prod"] = c.prod_only;
  d["svcs"] = c.services;
  d["vol"] = c.volume;
  d["qfrom"] = c.quiet_from;
  d["qto"] = c.quiet_to;
  d["ota"] = c.auto_update;
  d["mic"] = c.listen;
  d["snd_info"] = c.snd_info;
  d["snd_crit"] = c.snd_crit;
  d["snd_good"] = c.snd_good;
  d["talk"] = c.talk;
  d["has_ics"] = c.ics.length() > 0;  // the address is a secret: never sent back
  d["lat"] = c.lat;
  d["lon"] = c.lon;
  d["has_gh"] = c.gh_token.length() > 0;
  d["watch"] = c.watch;
  d["team_left"] = c.team_left;
  d["team_right"] = c.team_right;
  d["buddy"] = c.buddy;
  d["brk"] = c.break_min;
  d["focus"] = c.focus_min;
  d["bday"] = c.birthday;
  d["key"] = c.api_key;
  d["custom"] = sound::customSamples() > 0;
  JsonArray sounds = d["sounds"].to<JsonArray>();
  for (int i = 0; i < kSoundCount; i++) {
    JsonObject o = sounds.add<JsonObject>();
    o["name"] = kSoundNames[i].name;
    o["label"] = kSoundNames[i].label;
  }
  sendJson(200, d);
}

// Change any subset; secrets only when a new value is sent ("" clears the calendar or GitHub)
void setConfig() {
  NEED_SESSION();
  JsonDocument in;
  if (!readJson(in)) return;
  bool restart = false, volume = false;
  settings::lock();
  Settings &s = settings::get();
  auto str = [&](const char *k, String &field, bool *changed = nullptr) {
    if (!in[k].is<const char *>()) return;
    String v = in[k].as<const char *>();
    v.trim();
    if (changed && v != field) *changed = true;
    field = v;
  };
  auto num = [&](const char *k, int &field, int lo, int hi) {
    if (in[k].is<int>()) field = constrain(in[k].as<int>(), lo, hi);
  };
  auto flag = [&](const char *k, bool &field) {
    if (in[k].is<bool>()) field = in[k].as<bool>();
  };
  str("ssid", s.ssid, &restart);
  if (in["pass"].is<const char *>() && strlen(in["pass"])) s.pass = in["pass"].as<const char *>(), restart = true;
  str("url", s.url, &restart);
  while (s.url.endsWith("/")) s.url.remove(s.url.length() - 1);
  if (in["token"].is<const char *>() && strlen(in["token"])) s.token = in["token"].as<const char *>();
  flag("push", s.push);
  num("minlvl", s.min_level, 0, 2);
  flag("prod", s.prod_only);
  str("svcs", s.services);
  if (in["vol"].is<int>()) volume = true;
  num("vol", s.volume, 0, 100);
  num("qfrom", s.quiet_from, 0, 23);
  num("qto", s.quiet_to, 0, 23);
  flag("ota", s.auto_update);
  flag("mic", s.listen);
  str("snd_info", s.snd_info);
  str("snd_crit", s.snd_crit);
  str("snd_good", s.snd_good);
  flag("talk", s.talk);
  str("ics", s.ics);
  str("lat", s.lat);
  str("lon", s.lon);
  if (in["gh"].is<const char *>()) s.gh_token = in["gh"].as<const char *>();
  str("watch", s.watch);
  str("team_left", s.team_left);
  str("team_right", s.team_right);
  flag("buddy", s.buddy);
  num("brk", s.break_min, 0, 240);
  num("focus", s.focus_min, 1, 180);
  str("bday", s.birthday);
  const bool newkey = in["newkey"] | false;
  if (newkey) settings::newKey();
  settings::unlock();
  settings::save();      // writes a snapshot, outside the lock
  if (newkey) forget();  // a new key also signs every browser out
  if (volume) ask(Command::VOLUME, s.volume);
  goliash::refreshSoon();
  JsonDocument out;
  out["saved"] = true;
  out["restart"] = restart;  // Wi-Fi or Goliash address changed: takes a restart
  sendJson(200, out);
}

void getState() {
  NEED_SESSION();
  static const char *states[] = {"off", "connecting", "no Wi-Fi", "unreachable", "bad token", "ok"};
  JsonDocument d;
  d["version"] = GOLI_VERSION;
  d["uptime_s"] = millis() / 1000;
  d["ip"] = WiFi.localIP().toString();
  d["rssi"] = WiFi.RSSI();
  d["goliash"] = settings::hasGoliash() ? states[int(goliash::status())] : "not used";
  d["calendar"] = calendar::state();
  char w[40];
  weather::summary(w, sizeof w);
  d["weather"] = w;
  d["github"] = github::state();
  d["down"] = watch::down();
  bot::Meeting m[6];
  const int mn = calendar::upcoming(m, 6);
  JsonArray meetings = d["meetings"].to<JsonArray>();
  for (int i = 0; i < mn; i++) {
    JsonObject o = meetings.add<JsonObject>();
    o["title"] = m[i].title;
    o["start"] = m[i].start;
  }
  team::Peer p[8];
  const int pn = team::peers(p, 8);
  JsonArray peers = d["peers"].to<JsonArray>();
  for (int i = 0; i < pn; i++) peers.add(p[i].host);
  goliash::LogEntry l[20];
  const int ln = goliash::log(l, 20);
  JsonArray log = d["log"].to<JsonArray>();
  static const char *levels[] = {"good", "info", "critical"};
  for (int i = 0; i < ln; i++) {
    JsonObject o = log.add<JsonObject>();
    o["at"] = l[i].at;
    o["level"] = levels[int(l[i].level)];
    o["title"] = l[i].title;
  }
  sendJson(200, d);
}

void test() {
  NEED_SESSION();
  JsonDocument in;
  if (!readJson(in)) return;
  goliash::pushAlert(in["title"] | "Hello from the web page", parseLevel(in["level"] | "info"));
  reply(202, "ringing");
}

void play() {
  NEED_SESSION();
  JsonDocument in;
  if (!readJson(in)) return;
  sound::play(soundByName(in["sound"] | "", Sound::NONE));
  reply(200, "playing");
}

void action() {
  NEED_SESSION();
  JsonDocument in;
  if (!readJson(in)) return;
  const char *what = in["do"] | "";
  if (!strcmp(what, "poke")) ask(Command::POKE);
  else if (!strcmp(what, "focus")) ask(Command::FOCUS);
  else if (!strcmp(what, "demo")) ask(Command::DEMO);
  else if (!strcmp(what, "setup")) ask(Command::SETUP);
  else if (!strcmp(what, "restart")) {
    reply(200, "restarting");
    delay(300);
    ESP.restart();
  } else return reply(400, "unknown action");
  reply(200, "done");
}

// The custom sound: raw 16 kHz mono 16-bit PCM, converted by the page (any audio file works there)
int16_t *upload = nullptr;
size_t upload_bytes = 0;
bool upload_ok = false;

void soundChunk() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upload_ok = session();
    upload_bytes = 0;
    if (upload_ok && !upload) upload = (int16_t *)ps_malloc(synthBytes());
    upload_ok = upload_ok && upload;
  } else if (u.status == UPLOAD_FILE_WRITE && upload_ok) {
    const size_t room = synthBytes() - upload_bytes;
    const size_t n = u.currentSize < room ? u.currentSize : room;
    memcpy((uint8_t *)upload + upload_bytes, u.buf, n);  // bytes, not samples: chunks can be odd-sized
    upload_bytes += n;
  }
}

void soundDone() {
  const bool ok_upload = upload_ok && session();
  const size_t samples = upload_bytes / 2;
  upload_ok = false;  // nothing carries over to the next request
  upload_bytes = 0;
  if (!ok_upload) return reply(401, "pair this browser first");
  if (!samples) return reply(400, "empty sound");
  const bool ok = sound::setCustom(upload, samples);
  reply(ok ? 200 : 500, ok ? "saved" : "could not save");
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
  static const char *headers[] = {"Authorization", "X-Goliash-Signature", "X-Goliash-Timestamp", "Host"};
  server.collectHeaders(headers, 4);
  server.on("/", HTTP_GET, root);
  server.on("/status", HTTP_GET, status);
  server.on("/notify", HTTP_POST, notify);
  server.on("/api/pair/start", HTTP_POST, pairStart);
  server.on("/api/pair", HTTP_POST, pair);
  server.on("/api/config", HTTP_GET, getConfig);
  server.on("/api/config", HTTP_POST, setConfig);
  server.on("/api/state", HTTP_GET, getState);
  server.on("/api/test", HTTP_POST, test);
  server.on("/api/play", HTTP_POST, play);
  server.on("/api/do", HTTP_POST, action);
  server.on("/api/sound", HTTP_POST, soundDone, soundChunk);
  store.begin("goli-web", false);
  xTaskCreatePinnedToCore(task, "goli_api", 8192, nullptr, 1, nullptr, 0);
}

bool take(Request &out) { return requests && xQueueReceive(requests, &out, 0) == pdTRUE; }

const char *hostname() {
  if (!*host) {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // works before Wi-Fi is started
    snprintf(host, sizeof host, "goli-%02x%02x", mac[4], mac[5]);
  }
  return host;
}

}  // namespace api
