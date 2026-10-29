#include "calendar.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <time.h>

#include <algorithm>

#include "ics.h"
#include "settings.h"
#include "tls.h"

namespace calendar {
namespace {

const int kMax = ics::kMeetings;
bot::Meeting found[kMax];  // what other tasks read, under `lock`
int found_n = 0;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
const char *last_state = "off";

// Streams one calendar into the parser; returns what went wrong, or nullptr
const char *readOne(String url) {
  if (url.startsWith("webcal://")) url = "https://" + url.substring(9);
  NetworkClientSecure tls;
  trustPublicRoots(tls);
  NetworkClient plain;
  HTTPClient http;
  const bool https = url.startsWith("https://");
  if (!(https ? http.begin(tls, url) : http.begin(plain, url))) return "bad address";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.useHTTP10(true);  // a plain stream that ends when the server closes it (no chunks)
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("calendar: HTTP %d\n", code);
    http.end();
    return code == 404 || code == 403 || code == 401 ? "address refused" : "unreachable";
  }
  NetworkClient *stream = http.getStreamPtr();
  const int size = http.getSize();  // -1 when unknown
  int read = 0;
  bool complete = false;
  const uint32_t started = millis();
  while (millis() - started < 20000) {  // per calendar: the other sources wait meanwhile
    if (size >= 0 && read >= size) {
      complete = true;
      break;
    }
    if (!stream->available()) {
      if (!stream->connected()) {
        complete = size < 0;
        break;
      }
      delay(5);
      continue;
    }
    const String line = stream->readStringUntil('\n');
    read += line.length() + 1;
    ics::line(line);
  }
  ics::endOfFile();
  http.end();
  return complete ? nullptr : "too big or too slow";
}

}  // namespace

void poll() {
  const Settings cfg = settings::copy();
  if (!cfg.ics.length()) {
    last_state = "off";
    portENTER_CRITICAL(&lock);
    found_n = 0;
    portEXIT_CRITICAL(&lock);
    return;
  }
  const time_t now = time(nullptr);
  if (now < 1700000000) {
    last_state = "waiting for the time";
    return;
  }
  // Today's window, in local time: from a minute ago until midnight
  struct tm lt;
  localtime_r(&now, &lt);
  lt.tm_hour = 23, lt.tm_min = 59, lt.tm_sec = 59, lt.tm_isdst = -1;
  ics::begin(int64_t(now) - 60, int64_t(mktime(&lt)));

  // Several calendars (work, family...): addresses separated by spaces, commas or new lines
  const char *problem = nullptr;
  int calendars = 0;
  String all = cfg.ics;
  all.replace(",", " ");
  all.replace("\n", " ");
  for (int from = 0; from < int(all.length()) && calendars < 4;) {
    int space = all.indexOf(' ', from);
    if (space < 0) space = all.length();
    const String url = all.substring(from, space);
    from = space + 1;
    if (!url.length()) continue;
    calendars++;
    if (const char *p = readOne(url)) problem = p;
  }

  ics::Meeting m[kMax];
  const int n = ics::finish(m, kMax);
  portENTER_CRITICAL(&lock);
  for (int i = 0; i < n; i++) {
    memcpy(found[i].title, m[i].title, sizeof found[i].title);
    found[i].start = m[i].start;
    found[i].end = m[i].end;
  }
  found_n = n;
  portEXIT_CRITICAL(&lock);
  last_state = problem ? problem : "ok";
  Serial.printf("calendar: %d meetings left today from %d calendar(s)%s\n", n, calendars, problem ? " (with a problem)" : "");
}

int upcoming(bot::Meeting *out, int max) {
  portENTER_CRITICAL(&lock);
  const int n = std::min(found_n, max);
  for (int i = 0; i < n; i++) out[i] = found[i];
  portEXIT_CRITICAL(&lock);
  return n;
}

const char *state() { return last_state; }

}  // namespace calendar
