#include "goliash.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <time.h>

#include <algorithm>

#include "calendar.h"
#include "text.h"
#include "config.h"
#include "github.h"
#include "ota.h"
#include "rules.h"
#include "settings.h"
#include "team.h"
#include "watch.h"
#include "weather.h"

namespace goliash {
namespace {

QueueHandle_t alerts = nullptr;
volatile Status state = Status::OFF;
int64_t last_ms = 0;  // newest sent_at seen, Unix ms (0 = not polled yet)
Summary last_summary;
LogEntry history[20];
int history_n = 0, history_at = 0;
portMUX_TYPE history_lock = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE summary_lock = portMUX_INITIALIZER_UNLOCKED;

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return int64_t(era) * 146097 + int64_t(doe) - 719468;
}

// RFC 3339 ("2026-10-28T10:15:30.123456+01:00" or "...Z") -> Unix ms; 0 if unparsable.
int64_t parseTime(const char *s) {
  int Y, M, D, h, m, sec, n = 0;
  if (!s || sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &Y, &M, &D, &h, &m, &sec, &n) != 6) return 0;
  const char *p = s + n;
  int ms = 0;
  if (*p == '.') {
    int scale = 100;
    for (p++; isdigit(*p); p++, scale /= 10) ms += (*p - '0') * scale;
  }
  int off = 0;
  if (*p == '+' || *p == '-') {
    int oh = 0, om = 0;
    sscanf(p + 1, "%2d:%2d", &oh, &om);
    off = (oh * 60 + om) * (*p == '-' ? -1 : 1);
  }
  const int64_t secs = daysFromCivil(Y, M, D) * 86400 + h * 3600 + m * 60 + sec - off * 60;
  return secs * 1000 + ms;
}


rules::Filter filterOf(const Settings &cfg) {
  rules::Filter f;
  f.min_level = cfg.min_level;
  f.prod_only = cfg.prod_only;
  f.services = cfg.services;
  return f;
}

void queue(const char *title, int count, AlertLevel level, bool prod_deploy = false) {
  Alert a;
  a.count = count > 99 ? 99 : count;
  a.level = level;
  a.prod_deploy = prod_deploy;
  foldAscii(title, a.title, sizeof a.title);
  Serial.printf("alert: %d new, \"%s\"\n", count, a.title);
  xQueueSend(alerts, &a, 0);
  LogEntry e;
  const time_t now = time(nullptr);
  e.at = now > 1700000000 ? int64_t(now) : 0;
  e.level = level;
  snprintf(e.title, sizeof e.title, "%s", a.title);
  portENTER_CRITICAL(&history_lock);
  history[history_at] = e;
  history_at = (history_at + 1) % 20;
  if (history_n < 20) history_n++;
  portEXIT_CRITICAL(&history_lock);
}

// GET a Goliash API path into doc; updates the connection state. Returns true on 200 + valid JSON.
bool fetch(const Settings &cfg, const String &path, JsonDocument &doc, const JsonDocument *filter) {
  NetworkClientSecure tls;
  NetworkClient plain;
  const bool https = cfg.url.startsWith("https://");
  if (https) {
    if (cfg.ca.length()) tls.setCACert(cfg.ca.c_str());
    else tls.setInsecure();
  }
  const String url = cfg.url + path;
  HTTPClient http;
  if (!(https ? http.begin(tls, url) : http.begin(plain, url))) return false;
  http.addHeader("Authorization", "Bearer " + cfg.token);
  http.addHeader("Accept", "application/json");
  http.setTimeout(8000);
  http.useHTTP10(true);  // no chunked encoding: the JSON is read straight off the socket
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("goliash: %s HTTP %d\n", path.c_str(), code);
    state = code == 401 || code == 403 ? Status::BAD_TOKEN : Status::UNREACHABLE;
    http.end();
    return false;
  }
  const DeserializationError err = filter
                                       ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
                                       : deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("goliash: bad JSON from %s (%s)\n", path.c_str(), err.c_str());
    state = Status::UNREACHABLE;
    return false;
  }
  state = Status::OK;
  return true;
}

void poll() {
  const Settings cfg = settings::copy();
  // First poll only learns where we are; afterwards ask for what came after the newest one seen.
  char since[40];
  if (last_ms) {
    const time_t t = time_t(last_ms / 1000);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(since, sizeof since, "%Y-%m-%dT%H:%M:%SZ", &tm);
  } else {
    strcpy(since, "24h");
  }

  JsonDocument filter;
  filter[0]["sent_at"] = true;
  filter[0]["title"] = true;
  for (const char *f : {"type", "production", "environment", "service"}) filter[0]["items"][0][f] = true;
  JsonDocument doc;
  if (!fetch(cfg, String("/api/v1/notifications/sent?since=") + since, doc, &filter)) return;

  // Newest first
  int64_t newest = last_ms;
  int fresh = 0;
  const char *title = nullptr;
  AlertLevel level = AlertLevel::GOOD;
  bool prod = false;
  for (JsonObject m : doc.as<JsonArray>()) {
    const int64_t t = parseTime(m["sent_at"]);
    if (t > newest) newest = t;
    AlertLevel l;
    if (last_ms && t > last_ms && rules::judge(filterOf(cfg), m["items"], l, &prod)) {
      if (!title) title = m["title"] | "";
      level = fresh ? std::max(level, l) : l;
      fresh++;
    }
  }
  const bool first = last_ms == 0;
  const time_t now = time(nullptr);
  // Nothing sent yet and no clock: stay in "learn" mode rather than ask for everything since 1970
  last_ms = newest ? newest : (now > 1700000000 ? int64_t(now) * 1000 : 0);
  if (first || !fresh) return;
  queue(title, fresh, level, prod);
}

void pollSummary() {
  const Settings cfg = settings::copy();
  JsonDocument doc;
  if (!fetch(cfg, "/api/v1/summary", doc, nullptr)) return;
  Summary s;
  s.valid = true;
  foldAscii(doc["workspace"] | "", s.workspace, sizeof s.workspace);
  s.services = doc["services"] | 0;
  s.drifting = doc["drifting"] | 0;
  s.updates = doc["updates"] | 0;
  s.exploited = doc["exploited"] | 0;
  s.stale = doc["stale_targets"] | 0;
  s.at_ms = millis();
  portENTER_CRITICAL(&summary_lock);
  last_summary = s;
  portEXIT_CRITICAL(&summary_lock);
}

volatile bool refresh = false;  // settings changed: poll everything now

bool due(uint32_t &next, uint32_t every_ms) {
  if (!refresh && int32_t(millis() - next) < 0) return false;
  next = millis() + every_ms;
  return true;
}

// The network task: Wi-Fi, then every source in turn, each on its own schedule
void task(void *) {
  const Settings &cfg = settings::get();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  configTzTime(GOLI_TZ, "pool.ntp.org", "time.google.com");
  state = Status::CONNECTING;
  const uint32_t started = millis();
  uint32_t next_ota = OTA_FIRST_CHECK_S * 1000UL, next_summary = 0, next_poll = 0, next_cal = 0,
           next_weather = 0, next_gh = 0, next_watch = 0, next_hello = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - started > 45000) state = Status::NO_WIFI;
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    team::begin();
    const bool forced = refresh;
    if (!settings::hasGoliash()) {
      state = Status::OK;  // Goliash is optional
    } else {
      if (due(next_summary, GOLIASH_SUMMARY_S * 1000UL)) pollSummary();
      if (!settings::get().push && due(next_poll, GOLIASH_POLL_S * 1000UL)) poll();  // webhooks: Goliash calls
    }
    if (due(next_cal, 10 * 60000UL)) calendar::poll();
    if (due(next_weather, 30 * 60000UL)) weather::poll();
    if (due(next_gh, 60000UL)) github::poll();
    if (due(next_watch, 60000UL)) watch::poll();
    if (due(next_hello, 60000UL)) team::hello();
    if (forced) refresh = false;
    if (int32_t(millis() - next_ota) >= 0) {
      const bool ok = ota::check();  // restarts Goli when it installs an update
      next_ota = millis() + (ok ? OTA_EVERY_H * 3600000UL : 3600000UL);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

}  // namespace

bool configured() { return settings::hasWifi(); }

void begin() {
  if (!alerts) alerts = xQueueCreate(4, sizeof(Alert));
  if (configured()) xTaskCreatePinnedToCore(task, "goli_net", 16384, nullptr, 1, nullptr, 0);
}

Status status() { return state; }

bool takeAlert(Alert &out) { return alerts && xQueueReceive(alerts, &out, 0) == pdTRUE; }

bool pushMessage(JsonObjectConst msg) {
  AlertLevel level;
  bool prod = false;
  JsonArrayConst items = msg["items"];
  if (!alerts || !rules::judge(filterOf(settings::copy()), items, level, &prod)) return false;
  char title[96];
  if (items.size() == 1) snprintf(title, sizeof title, "%s", (const char *)(items[0]["text"] | "Goliash"));
  else snprintf(title, sizeof title, "%u updates in %s", unsigned(items.size()), (const char *)(msg["workspace"] | "Goliash"));
  queue(title, 1, level, prod);
  return true;
}

bool pushAlert(const char *title, AlertLevel level, int count) {
  if (!alerts) return false;
  if (settings::get().min_level >= int(AlertLevel::CRITICAL) && level != AlertLevel::CRITICAL) return false;
  queue(title, count, level);
  return true;
}

void refreshSoon() { refresh = true; }

int log(LogEntry *out, int max) {
  portENTER_CRITICAL(&history_lock);
  const int n = history_n < max ? history_n : max;
  for (int i = 0; i < n; i++) out[i] = history[(history_at - 1 - i + 20) % 20];
  portEXIT_CRITICAL(&history_lock);
  return n;
}

bool summary(Summary &out) {
  portENTER_CRITICAL(&summary_lock);
  out = last_summary;
  portEXIT_CRITICAL(&summary_lock);
  return out.valid;
}

bool quietHours() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;  // clock not set yet
  struct tm tm;
  localtime_r(&now, &tm);
  const int h = tm.tm_hour;
  const int from = settings::get().quiet_from, to = settings::get().quiet_to;
  if (from == to) return false;  // no quiet hours
  return from > to ? (h >= from || h < to) : (h >= from && h < to);
}

}  // namespace goliash
