#include "improv.h"

#include <WiFi.h>

#include "config.h"
#include "settings.h"

namespace improv {
namespace {

enum : uint8_t { TYPE_STATE = 1, TYPE_ERROR = 2, TYPE_RPC = 3, TYPE_RESULT = 4 };
enum : uint8_t { STATE_READY = 2, STATE_PROVISIONING = 3, STATE_PROVISIONED = 4 };
enum : uint8_t { ERR_NONE = 0, ERR_INVALID = 1, ERR_UNKNOWN_CMD = 2, ERR_CONNECT = 3 };
enum : uint8_t { CMD_WIFI = 1, CMD_STATE = 2, CMD_INFO = 3, CMD_SCAN = 4 };

const char kHeader[] = "IMPROV";
const uint32_t kConnectTimeoutMs = 20000;

UrlFn url_fn = nullptr;
DoneFn done_fn = nullptr;

uint8_t buf[256];
size_t len = 0;

// Credentials being tried
String try_ssid, try_pass;
uint32_t try_since = 0;
bool trying = false, provisioned = false;

void send(uint8_t type, const uint8_t *data, size_t n) {
  uint8_t out[6 + 3 + 255 + 1];
  memcpy(out, kHeader, 6);
  out[6] = 1;  // protocol version
  out[7] = type;
  out[8] = uint8_t(n);
  memcpy(out + 9, data, n);
  uint8_t sum = 0;
  for (size_t i = 0; i < 9 + n; i++) sum += out[i];
  out[9 + n] = sum;
  Serial.write(out, 10 + n);
  Serial.write('\n');
}

void sendState(uint8_t state) { send(TYPE_STATE, &state, 1); }
void sendError(uint8_t err) { send(TYPE_ERROR, &err, 1); }

// An RPC result: the command, then a list of length-prefixed strings
void sendResult(uint8_t cmd, std::initializer_list<String> strings) {
  uint8_t data[255];
  size_t n = 2;
  for (const String &s : strings) {
    const size_t l = std::min<size_t>(s.length(), 255 - n - 1);
    data[n++] = uint8_t(l);
    memcpy(data + n, s.c_str(), l);
    n += l;
  }
  data[0] = cmd;
  data[1] = uint8_t(n - 2);
  send(TYPE_RESULT, data, n);
}

bool online() { return WiFi.status() == WL_CONNECTED; }

void sendProvisioned(uint8_t cmd) {
  sendState(STATE_PROVISIONED);
  const String url = url_fn ? url_fn() : String();
  if (url.length()) sendResult(cmd, {url});
  else sendResult(cmd, {});
}

void handleRpc(const uint8_t *d, size_t n) {
  if (n < 2 || d[1] + 2u > n) return sendError(ERR_INVALID);
  const uint8_t cmd = d[0];
  const uint8_t *p = d + 2;
  switch (cmd) {
    case CMD_WIFI: {
      const uint8_t sl = p[0];
      if (sl + 2u > d[1]) return sendError(ERR_INVALID);
      const uint8_t pl = p[1 + sl];
      if (sl + pl + 2u > d[1]) return sendError(ERR_INVALID);
      try_ssid = String((const char *)p + 1, sl);
      try_pass = String((const char *)p + 2 + sl, pl);
      sendError(ERR_NONE);
      sendState(STATE_PROVISIONING);
      WiFi.mode(WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ? WIFI_AP_STA : WIFI_STA);
      WiFi.disconnect();
      WiFi.begin(try_ssid.c_str(), try_pass.c_str());
      trying = true;
      try_since = millis();
      return;
    }
    case CMD_STATE:
      sendError(ERR_NONE);
      if (provisioned || (settings::hasWifi() && online())) sendProvisioned(CMD_STATE);
      else sendState(STATE_READY);
      return;
    case CMD_INFO:
      sendError(ERR_NONE);
      sendResult(CMD_INFO, {"Goli desk bot", GOLI_VERSION, "ESP32-S3", "Goli"});
      return;
    case CMD_SCAN: {
      sendError(ERR_NONE);
      const int found = WiFi.scanNetworks();
      for (int i = 0; i < found; i++) {
        if (!WiFi.SSID(i).length()) continue;
        sendResult(CMD_SCAN, {WiFi.SSID(i), String(WiFi.RSSI(i)),
                              WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
      }
      WiFi.scanDelete();
      sendResult(CMD_SCAN, {});  // end of the list
      return;
    }
    default:
      sendError(ERR_UNKNOWN_CMD);
  }
}

void handle(uint8_t type, const uint8_t *d, size_t n) {
  if (type == TYPE_RPC) handleRpc(d, n);
}

// Feeds one byte; a packet is IMPROV, version, type, length, data, checksum.
void feed(uint8_t c) {
  if (len < 6) {
    if (c == uint8_t(kHeader[len])) buf[len++] = c;
    else len = c == 'I' ? (buf[0] = c, 1) : 0;
    return;
  }
  buf[len++] = c;
  if (len < 9) return;
  const size_t total = 9 + buf[8] + 1;
  if (len < total) return;
  uint8_t sum = 0;
  for (size_t i = 0; i < total - 1; i++) sum += buf[i];
  if (buf[6] == 1 && sum == buf[total - 1]) handle(buf[7], buf + 9, buf[8]);
  len = 0;
}

}  // namespace

void begin(UrlFn url, DoneFn done) {
  url_fn = url;
  done_fn = done;
}

void loop() {
  while (Serial.available()) feed(uint8_t(Serial.read()));

  if (!trying) return;
  if (online() && WiFi.SSID() == try_ssid) {
    trying = false;
    provisioned = true;
    Settings &s = settings::get();
    s.ssid = try_ssid;
    s.pass = try_pass;
    settings::save();
    Serial.printf("improv: joined %s, %s\n", try_ssid.c_str(), WiFi.localIP().toString().c_str());
    sendProvisioned(CMD_WIFI);
    if (done_fn) done_fn();
  } else if (millis() - try_since > kConnectTimeoutMs) {
    trying = false;
    WiFi.disconnect();
    sendError(ERR_CONNECT);
    sendState(STATE_READY);
    // Back to the saved network, if any
    if (settings::hasWifi() && WiFi.getMode() == WIFI_STA)
      WiFi.begin(settings::get().ssid.c_str(), settings::get().pass.c_str());
  }
}

}  // namespace improv
