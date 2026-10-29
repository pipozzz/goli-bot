#include "github.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "goliash.h"
#include "settings.h"
#include "tls.h"

namespace github {
namespace {

String last_modified;
bool learned = false;        // the first poll only learns what is already there
uint32_t token_hash = 0;     // a new token starts over (another account's backlog shouldn't ring)
char seen[24][48];           // "id@updated_at" already rung for: a thread can ring again when it changes
int seen_at = 0;
const char *last_state = "off";

bool wasSeen(const char *id) {
  for (auto &s : seen)
    if (!strcmp(s, id)) return true;
  snprintf(seen[seen_at], sizeof seen[seen_at], "%s", id);
  seen_at = (seen_at + 1) % 24;
  return false;
}

}  // namespace

void poll() {
  const Settings cfg = settings::copy();
  if (!cfg.gh_token.length()) {
    last_state = "off";
    return;
  }
  uint32_t h = 2166136261u;
  for (char c : cfg.gh_token) h = (h ^ uint8_t(c)) * 16777619u;
  if (h != token_hash) {
    token_hash = h;
    learned = false;
    last_modified = "";
    memset(seen, 0, sizeof seen);
  }
  NetworkClientSecure tls;
  trustPublicRoots(tls);
  HTTPClient http;
  if (!http.begin(tls, "https://api.github.com/notifications?participating=true&per_page=20")) return;
  http.addHeader("Authorization", "Bearer " + cfg.gh_token);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  http.setUserAgent("goli-bot");
  if (last_modified.length()) http.addHeader("If-Modified-Since", last_modified);
  const char *keep[] = {"Last-Modified"};
  http.collectHeaders(keep, 1);
  http.setTimeout(10000);
  http.useHTTP10(true);
  const int code = http.GET();
  if (code == 304) {  // nothing new (and it didn't count against the rate limit)
    last_state = "ok";
    http.end();
    return;
  }
  if (code != 200) {
    Serial.printf("github: HTTP %d\n", code);
    last_state = code == 401 || code == 403 ? "token refused" : "unreachable";
    http.end();
    return;
  }
  const String modified = http.header("Last-Modified");

  JsonDocument filter;
  filter[0]["id"] = true;
  filter[0]["updated_at"] = true;
  filter[0]["reason"] = true;
  filter[0]["subject"]["title"] = true;
  filter[0]["repository"]["name"] = true;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) return;
  last_modified = modified;  // only once this answer was read: else it would never come again
  last_state = "ok";

  for (JsonObject n : doc.as<JsonArray>()) {
    char id[48];
    snprintf(id, sizeof id, "%s@%s", (const char *)(n["id"] | ""), (const char *)(n["updated_at"] | ""));
    if (wasSeen(id) || !learned) continue;
    const char *reason = n["reason"] | "";
    const char *repo = n["repository"]["name"] | "";
    const char *title = n["subject"]["title"] | "";
    char text[96];
    AlertLevel level = AlertLevel::INFO;
    if (!strcmp(reason, "ci_activity")) {
      snprintf(text, sizeof text, "CI failed: %s", repo);
      level = AlertLevel::CRITICAL;
    } else if (!strcmp(reason, "security_alert")) {
      snprintf(text, sizeof text, "security: %s", repo);
      level = AlertLevel::CRITICAL;
    } else if (!strcmp(reason, "review_requested") || !strcmp(reason, "approval_requested")) {
      snprintf(text, sizeof text, "review: %s", title);
    } else if (!strcmp(reason, "mention") || !strcmp(reason, "team_mention") || !strcmp(reason, "assign")) {
      snprintf(text, sizeof text, "%s: %s", repo, title);
    } else {
      continue;  // comments on threads you're in etc.: GitHub's own inbox is enough
    }
    goliash::pushAlert(text, level);
  }
  learned = true;
}

const char *state() { return last_state; }

}  // namespace github
