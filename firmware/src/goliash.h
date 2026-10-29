#pragma once

#include <stdint.h>

#include <ArduinoJson.h>

#include "face.h"  // AlertLevel

// Watches Goliash for new notifications (the same list as the bell in the Goliash UI).
// Wi-Fi and HTTP run in their own task so the face never stutters.
namespace goliash {

struct Alert {
  uint8_t count;      // new messages since the last poll
  AlertLevel level;   // the worst of them
  bool prod_deploy;   // one of them is a release reaching production
  char title[96];     // title of the newest one, folded to ASCII for the built-in font
};

// Goliash at a glance (GET /api/v1/summary), for the overview screen
struct Summary {
  bool valid = false;
  char workspace[32] = "";
  int services = 0, drifting = 0, updates = 0, exploited = 0, stale = 0;
  uint32_t at_ms = 0;
};

enum class Status { OFF, CONNECTING, NO_WIFI, UNREACHABLE, BAD_TOKEN, OK };

bool configured();     // Wi-Fi and Goliash are set up
void begin();          // starts the network task (Goliash and every other source) when Wi-Fi is set up
Status status();
bool takeAlert(Alert &out);  // call from loop(); true when something new arrived
bool quietHours();     // no sounds at night (quiet hours from setup)
bool summary(Summary &out);  // false until the first summary arrived
void refreshSoon();          // settings changed: poll every source again now

struct LogEntry {
  int64_t at;        // Unix seconds (0 if the clock wasn't set yet)
  AlertLevel level;
  char title[64];
};
int log(LogEntry *out, int max);  // the latest alerts, newest first

// From the local API (any task that runs the web server): a Goliash webhook body (a notifier
// message, filtered like polled ones), or a plain alert. False when the filter drops it.
bool pushMessage(JsonObjectConst msg);
bool pushAlert(const char *title, AlertLevel level, int count = 1);

}  // namespace goliash
