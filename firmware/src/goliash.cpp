#include "goliash.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define GOLI_ONLINE 1
#else
#define GOLI_ONLINE 0
#endif

namespace goliash {
namespace {

QueueHandle_t alerts = nullptr;
int64_t last_ms = 0;  // newest sent_at seen, Unix ms (0 = not polled yet)

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

// The built-in font is ASCII only: fold common accented letters (Slovak, Czech, German...).
void foldAscii(const char *in, char *out, size_t cap) {
  static const char *from[] = {"á", "ä", "à", "â", "č", "ď", "é", "ě", "è", "ê", "í", "ĺ", "ľ", "ň", "ó", "ô", "ö", "ŕ",
                               "ř", "š", "ť", "ú", "ů", "ü", "ý", "ž", "Á", "Ä", "Č", "Ď", "É", "Ě", "Í", "Ĺ", "Ľ", "Ň",
                               "Ó", "Ô", "Ö", "Ŕ", "Ř", "Š", "Ť", "Ú", "Ů", "Ü", "Ý", "Ž", "ß", "–", "—", "’", "“", "”", "…"};
  static const char *to[] = {"a", "a", "a", "a", "c", "d", "e", "e", "e", "e", "i", "l", "l", "n", "o", "o", "o", "r",
                             "r", "s", "t", "u", "u", "u", "y", "z", "A", "A", "C", "D", "E", "E", "I", "L", "L", "N",
                             "O", "O", "O", "R", "R", "S", "T", "U", "U", "U", "Y", "Z", "ss", "-", "-", "'", "\"", "\"", "..."};
  size_t o = 0;
  while (*in && o + 4 < cap) {
    if ((uint8_t)*in < 0x80) {
      out[o++] = *in++;
      continue;
    }
    bool hit = false;
    for (size_t k = 0; k < sizeof from / sizeof *from; k++) {
      const size_t len = strlen(from[k]);
      if (!strncmp(in, from[k], len)) {
        for (const char *t = to[k]; *t && o + 1 < cap;) out[o++] = *t++;
        in += len;
        hit = true;
        break;
      }
    }
    if (!hit) {  // unknown character: skip the whole UTF-8 sequence
      out[o++] = '?';
      for (in++; ((uint8_t)*in & 0xC0) == 0x80; in++) {
      }
    }
  }
  out[o] = 0;
}

#if GOLI_ONLINE
void poll() {
  NetworkClientSecure tls;
  NetworkClient plain;
  const bool https = !strncmp(GOLIASH_URL, "https://", 8);
  if (https) {
    if (strlen(GOLIASH_ROOT_CA)) tls.setCACert(GOLIASH_ROOT_CA);
    else tls.setInsecure();
  }

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
  String url = String(GOLIASH_URL) + "/api/v1/notifications/sent?since=" + since;

  HTTPClient http;
  if (!(https ? http.begin(tls, url) : http.begin(plain, url))) return;
  http.addHeader("Authorization", "Bearer " GOLIASH_TOKEN);
  http.addHeader("Accept", "application/json");
  http.setTimeout(8000);
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("goliash: HTTP %d\n", code);
    http.end();
    return;
  }

  JsonDocument filter;
  filter[0]["sent_at"] = true;
  filter[0]["title"] = true;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("goliash: bad JSON (%s)\n", err.c_str());
    return;
  }

  // Newest first
  int64_t newest = last_ms;
  int fresh = 0;
  const char *title = nullptr;
  for (JsonObject m : doc.as<JsonArray>()) {
    const int64_t t = parseTime(m["sent_at"]);
    if (t > newest) newest = t;
    if (last_ms && t > last_ms) {
      if (!title) title = m["title"] | "";
      fresh++;
    }
  }
  const bool first = last_ms == 0;
  const time_t now = time(nullptr);
  // Nothing sent yet and no clock: stay in "learn" mode rather than ask for everything since 1970
  last_ms = newest ? newest : (now > 1700000000 ? int64_t(now) * 1000 : 0);
  if (first || !fresh) return;

  Alert a;
  a.count = fresh > 99 ? 99 : fresh;
  foldAscii(title, a.title, sizeof a.title);
  Serial.printf("goliash: %d new, \"%s\"\n", fresh, a.title);
  xQueueSend(alerts, &a, 0);
}

void task(void *) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTzTime(GOLI_TZ, "pool.ntp.org", "time.google.com");
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) poll();
    vTaskDelay(pdMS_TO_TICKS(GOLIASH_POLL_S * 1000));
  }
}
#endif

}  // namespace

bool configured() { return GOLI_ONLINE; }

void begin() {
  alerts = xQueueCreate(4, sizeof(Alert));
#if GOLI_ONLINE
  xTaskCreatePinnedToCore(task, "goli_net", 12288, nullptr, 1, nullptr, 0);
#endif
}

bool takeAlert(Alert &out) { return alerts && xQueueReceive(alerts, &out, 0) == pdTRUE; }

bool quietHours() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;  // clock not set yet
  struct tm tm;
  localtime_r(&now, &tm);
  const int h = tm.tm_hour;
  return QUIET_FROM > QUIET_TO ? (h >= QUIET_FROM || h < QUIET_TO) : (h >= QUIET_FROM && h < QUIET_TO);
}

}  // namespace goliash
