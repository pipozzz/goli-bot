#pragma once

#include <ArduinoJson.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "WString.h"  // host tests: test/shim
#endif

#include "alert.h"

// What rings and how loud: Goliash notification items and the webhook formats Goli understands.
// No networking here, so the host tests cover it.
namespace rules {

// The filter from setup: everything or critical only, only production, only some services
// (comma-separated, empty = all). Items without an environment or a service always pass.
struct Filter {
  int min_level = 0;
  bool prod_only = false;
  String services;
};

AlertLevel levelOf(JsonObjectConst item);  // a Goliash notifier item
bool wanted(const Filter &f, JsonObjectConst item, AlertLevel level);
bool prodDeploy(JsonObjectConst item);     // a release that reached production
// The worst level among the items that pass the filter; false when none does
bool judge(const Filter &f, JsonArrayConst items, AlertLevel &level, bool *prod = nullptr);

AlertLevel parseLevel(const char *s);      // "critical", "good", anything else = info

// A POST /notify body
struct Hook {
  enum Kind { GOLIASH, ALERTMANAGER, UPTIME_KUMA, PLAIN, INVALID } kind = INVALID;
  char title[96] = "";
  AlertLevel level = AlertLevel::INFO;
  int count = 1;
  const char *error = nullptr;  // for INVALID
};
Hook webhook(JsonObjectConst msg);  // GOLIASH: hand the message to goliash::pushMessage

// One ntfy "message" event ({"title", "message", "priority", "tags"}). A message that is itself JSON
// (a monitoring tool posting its webhook to the topic) is read like POST /notify; otherwise the
// priority and tags give the level: 5 or rotating_light/warning/x = critical, white_check_mark/tada/+1 = good.
// `inner` holds the parsed message when the result is GOLIASH (for goliash::pushMessage).
Hook ntfy(JsonObjectConst event, JsonDocument &inner);

}  // namespace rules
