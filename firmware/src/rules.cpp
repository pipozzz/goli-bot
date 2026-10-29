#include "rules.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <algorithm>

namespace rules {

AlertLevel levelOf(JsonObjectConst item) {
  const char *type = item["type"] | "";
  static const char *critical[] = {"agent_stale", "vulnerability", "security_release", "unexpected_signer",
                                   "evidence_lost", "error"};
  for (const char *c : critical)
    if (!strcmp(type, c)) return AlertLevel::CRITICAL;
  if (!strcmp(type, "drift_detected")) return (item["production"] | false) ? AlertLevel::CRITICAL : AlertLevel::INFO;
  if (!strcmp(type, "agent_back") || !strcmp(type, "drift_resolved")) return AlertLevel::GOOD;
  return AlertLevel::INFO;
}

bool wanted(const Filter &f, JsonObjectConst item, AlertLevel level) {
  if (f.min_level >= int(AlertLevel::CRITICAL) && level != AlertLevel::CRITICAL) return false;
  if (f.prod_only && item["environment"].is<const char *>() && !(item["production"] | false)) return false;
  const char *svc = item["service"] | "";
  if (!f.services.length() || !*svc) return true;
  for (int from = 0; from <= int(f.services.length());) {
    int comma = f.services.indexOf(',', from);
    if (comma < 0) comma = f.services.length();
    String name = f.services.substring(from, comma);
    name.trim();
    if (!strcasecmp(name.c_str(), svc)) return true;
    from = comma + 1;
  }
  return false;
}

bool prodDeploy(JsonObjectConst item) {
  const char *type = item["type"] | "";
  return (item["production"] | false) && (!strcmp(type, "deployed") || !strcmp(type, "rollout"));
}

bool judge(const Filter &f, JsonArrayConst items, AlertLevel &level, bool *prod) {
  bool any = false;
  for (JsonObjectConst item : items) {
    const AlertLevel l = levelOf(item);
    if (!wanted(f, item, l)) continue;
    if (prod && prodDeploy(item)) *prod = true;
    level = any ? std::max(level, l) : l;
    any = true;
  }
  return any;
}

AlertLevel parseLevel(const char *s) {
  if (!strcmp(s, "critical")) return AlertLevel::CRITICAL;
  if (!strcmp(s, "good")) return AlertLevel::GOOD;
  return AlertLevel::INFO;
}

namespace {

// Alertmanager (Prometheus) and Grafana's alerting both post {"status", "alerts": [{labels, annotations}]}
void alertmanager(JsonObjectConst msg, Hook &h) {
  JsonArrayConst list = msg["alerts"];
  JsonObjectConst first = list[0];
  const bool resolved = !strcmp(msg["status"] | (first["status"] | "firing"), "resolved");
  int firing = 0;
  h.level = AlertLevel::INFO;
  for (JsonObjectConst a : list) {
    if (!resolved && !strcmp(a["status"] | "firing", "resolved")) continue;  // already over, sent along with the group
    firing++;
    const char *sev = a["labels"]["severity"] | "";
    if (!strcmp(sev, "critical") || !strcmp(sev, "page") || !strcmp(sev, "error")) h.level = AlertLevel::CRITICAL;
  }
  if (resolved) h.level = AlertLevel::GOOD;
  const char *what = msg["title"] | (first["annotations"]["summary"] | (first["labels"]["alertname"] | "alert"));
  snprintf(h.title, sizeof h.title, "%s%s", what, resolved && !msg["title"].is<const char *>() ? " - resolved" : "");
  h.count = std::max(1, resolved ? int(list.size()) : firing);
}

// Uptime Kuma: {"heartbeat": {"status": 0 down | 1 up | 2 pending | 3 maintenance}, "monitor": {"name"}, "msg"};
// its test button and certificate notices send heartbeat: null and only a message
void uptimeKuma(JsonObjectConst msg, Hook &h) {
  if (!msg["heartbeat"].is<JsonObjectConst>()) {
    snprintf(h.title, sizeof h.title, "%s", (const char *)(msg["msg"] | "Uptime Kuma"));
    h.level = AlertLevel::INFO;
    return;
  }
  const char *name = msg["monitor"]["name"] | "a monitor";
  const int status = msg["heartbeat"]["status"] | -1;
  if (status == 0 || status == 1) snprintf(h.title, sizeof h.title, "%s is %s", name, status == 1 ? "up" : "down");
  else snprintf(h.title, sizeof h.title, "%s: %s", name, status == 3 ? "maintenance" : "pending");
  h.level = status == 1 ? AlertLevel::GOOD : status == 0 ? AlertLevel::CRITICAL : AlertLevel::INFO;
}

}  // namespace

Hook webhook(JsonObjectConst msg) {
  Hook h;
  if (msg["items"].is<JsonArrayConst>()) {
    h.kind = Hook::GOLIASH;
  } else if (msg["alerts"].is<JsonArrayConst>() && msg["alerts"].size()) {
    h.kind = Hook::ALERTMANAGER;
    alertmanager(msg, h);
  } else if (msg["heartbeat"].is<JsonObjectConst>() ||
             (msg["msg"].is<const char *>() && !msg["title"].is<const char *>())) {
    h.kind = Hook::UPTIME_KUMA;
    uptimeKuma(msg, h);
  } else if (msg["title"].is<const char *>() && *(const char *)msg["title"]) {
    h.kind = Hook::PLAIN;
    snprintf(h.title, sizeof h.title, "%s", (const char *)msg["title"]);
    h.level = parseLevel(msg["level"] | "info");
    const char *state = msg["state"] | "";  // Grafana's legacy alerts: "alerting" / "ok"
    if (!strcmp(state, "alerting")) h.level = AlertLevel::CRITICAL;
    else if (!strcmp(state, "ok")) h.level = AlertLevel::GOOD;
  } else {
    h.error = "title is required";
  }
  return h;
}

Hook ntfy(JsonObjectConst event, JsonDocument &inner) {
  const char *message = event["message"] | "";
  const char *title = event["title"] | "";
  if (*message == '{' && !deserializeJson(inner, message) && inner.is<JsonObject>()) {
    Hook h = webhook(inner.as<JsonObjectConst>());
    if (h.kind != Hook::INVALID) return h;
  }
  Hook h;
  h.kind = Hook::PLAIN;
  if (*title && *message) snprintf(h.title, sizeof h.title, "%s: %s", title, message);
  else snprintf(h.title, sizeof h.title, "%s", *title ? title : *message ? message : "ntfy");
  const int priority = event["priority"] | 3;
  h.level = priority >= 5 ? AlertLevel::CRITICAL : AlertLevel::INFO;
  static const char *bad[] = {"rotating_light", "warning", "x", "red_circle", "skull", "fire"};
  static const char *good[] = {"white_check_mark", "heavy_check_mark", "tada", "+1", "green_circle", "partying_face"};
  for (const char *tag : event["tags"].as<JsonArrayConst>()) {
    if (!tag) continue;
    for (const char *b : bad)
      if (!strcmp(tag, b)) h.level = AlertLevel::CRITICAL;
    for (const char *g : good)
      if (!strcmp(tag, g) && h.level != AlertLevel::CRITICAL) h.level = AlertLevel::GOOD;
  }
  return h;
}

}  // namespace rules
