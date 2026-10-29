#include "team.h"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <WiFi.h>

#include <algorithm>

#include "api.h"
#include "settings.h"

namespace team {
namespace {

const uint16_t kPort = 47110;
AsyncUDP udp;
bool started = false;
QueueHandle_t glances = nullptr;
const int kMaxPeers = 8;
Peer known[kMaxPeers];
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

void send(const char *what) {
  if (!started) return;
  char msg[64];
  snprintf(msg, sizeof msg, "GOLI1 %s %s", what, api::hostname());
  udp.broadcastTo(msg, kPort);
}

void remember(const char *host) {
  portENTER_CRITICAL(&lock);
  int slot = -1, oldest = 0;
  for (int i = 0; i < kMaxPeers; i++) {
    if (!strcmp(known[i].host, host)) slot = i;
    if (known[i].seen_ms < known[oldest].seen_ms) oldest = i;
  }
  if (slot < 0) slot = oldest;
  snprintf(known[slot].host, sizeof known[slot].host, "%s", host);
  known[slot].seen_ms = millis() | 1;
  portEXIT_CRITICAL(&lock);
}

void received(AsyncUDPPacket &p) {
  char msg[64];
  const size_t n = std::min(p.length(), sizeof msg - 1);
  memcpy(msg, p.data(), n);
  msg[n] = 0;
  char what[8], host[16];
  if (sscanf(msg, "GOLI1 %7s %15s", what, host) != 2 || !strcmp(host, api::hostname())) return;
  remember(host);
  if (strcmp(what, "rang")) return;
  settings::lock();  // just the two names, not a copy of everything
  const Settings &cfg = settings::get();
  const int dir = cfg.team_left == host ? -1 : cfg.team_right == host ? 1 : 0;
  settings::unlock();
  if (dir) xQueueSend(glances, &dir, 0);
}

}  // namespace

void begin() {
  if (started) return;
  if (!glances) glances = xQueueCreate(4, sizeof(int));
  if (!udp.listen(kPort)) return;
  udp.onPacket(received);
  started = true;
  send("hello");
}

void hello() { send("hello"); }

void rang(int) { send("rang"); }

bool takeGlance(int &dir) { return glances && xQueueReceive(glances, &dir, 0) == pdTRUE; }

int peers(Peer *out, int max) {
  int n = 0;
  portENTER_CRITICAL(&lock);
  for (int i = 0; i < kMaxPeers && n < max; i++)
    if (known[i].seen_ms && millis() - known[i].seen_ms < 5 * 60000UL) out[n++] = known[i];
  portEXIT_CRITICAL(&lock);
  return n;
}

}  // namespace team
