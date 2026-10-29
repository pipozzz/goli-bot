#include "watch.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "goliash.h"
#include "settings.h"

namespace watch {
namespace {

const int kMax = 6;
struct Site {
  String url;
  int fails = 0;
  bool down = false;
};
Site sites[kMax];
int site_n = 0;
volatile int down_n = 0;

// "https://shop.example.com/health" -> "shop.example.com"
String hostOf(const String &url) {
  int from = url.indexOf("://");
  from = from < 0 ? 0 : from + 3;
  int to = url.indexOf('/', from);
  return url.substring(from, to < 0 ? url.length() : to);
}

bool up(const String &url) {
  // Only whether it answers: no secrets go out, so the certificate isn't checked here
  NetworkClientSecure tls;
  tls.setInsecure();
  NetworkClient plain;
  HTTPClient http;
  const bool https = url.startsWith("https://");
  if (!(https ? http.begin(tls, url) : http.begin(plain, url))) return false;
  http.setTimeout(8000);
  http.setUserAgent("goli-bot");
  const int code = http.GET();
  http.end();
  return code >= 200 && code < 400;
}

}  // namespace

void poll() {
  const Settings cfg = settings::copy();
  // Re-read the list (it may have changed on the web page); keep the state of URLs that stayed
  Site next[kMax];
  int n = 0;
  for (int from = 0; from < int(cfg.watch.length()) && n < kMax;) {
    int nl = cfg.watch.indexOf('\n', from);
    if (nl < 0) nl = cfg.watch.length();
    String url = cfg.watch.substring(from, nl);
    url.trim();
    from = nl + 1;
    if (!url.startsWith("http://") && !url.startsWith("https://")) continue;
    next[n].url = url;
    for (int i = 0; i < site_n; i++)
      if (sites[i].url == url) next[n] = sites[i];
    n++;
  }
  site_n = n;
  int down_now = 0;
  for (int i = 0; i < site_n; i++) {
    sites[i] = next[i];
    Site &s = sites[i];
    const bool ok = up(s.url);
    s.fails = ok ? 0 : s.fails + 1;
    const String host = hostOf(s.url);
    if (!s.down && s.fails >= 2) {
      s.down = true;
      goliash::pushAlert((host + " is down").c_str(), AlertLevel::CRITICAL);
    } else if (s.down && ok) {
      s.down = false;
      goliash::pushAlert((host + " is back").c_str(), AlertLevel::GOOD);
    }
    down_now += s.down;
  }
  down_n = down_now;
}

int down() { return down_n; }

}  // namespace watch
