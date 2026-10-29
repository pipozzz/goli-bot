#include "mqtt.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "api.h"
#include "goliash.h"
#include "settings.h"

namespace mqtt {
namespace {

NetworkClient net;
PubSubClient client(net);
volatile const char *last_status = "off";
String base;  // goli/<name>

bot::State shared;
bool shared_set = false;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

AlertLevel levelOf(const char *s) {
  return !strcmp(s, "critical") ? AlertLevel::CRITICAL : !strcmp(s, "good") ? AlertLevel::GOOD : AlertLevel::INFO;
}

void received(char *topic, uint8_t *payload, unsigned int len) {
  String t = topic, body;
  body.reserve(len);
  for (unsigned i = 0; i < len; i++) body += char(payload[i]);
  if (t == base + "/cmd") {
    if (body == "poke") api::request(api::Command::POKE);
    else if (body == "focus") api::request(api::Command::FOCUS);
    else if (body == "demo") api::request(api::Command::DEMO);
  } else if (t == base + "/say") {  // Home Assistant's notify entity: the message, as plain text
    if (body.length()) goliash::pushAlert(body.c_str(), AlertLevel::INFO);
  } else if (t == base + "/notify") {  // {"title": "...", "level": "info|critical|good"}
    JsonDocument doc;
    if (deserializeJson(doc, body)) return;
    const char *title = doc["title"] | "";
    if (*title) goliash::pushAlert(title, levelOf(doc["level"] | "info"));
  }
}

// One Home Assistant entity, announced with a retained config message
void announce(const char *component, const char *id, const char *name, JsonDocument &extra) {
  JsonDocument d = extra;
  d["name"] = name;
  d["uniq_id"] = String(api::hostname()) + "_" + id;
  d["avty_t"] = base + "/status";
  JsonObject dev = d["dev"].to<JsonObject>();
  dev["ids"].to<JsonArray>().add(api::hostname());
  dev["name"] = String("Goli ") + api::hostname();
  dev["mdl"] = "Goli desk bot";
  dev["mf"] = "Goliash";
  dev["sw"] = GOLI_VERSION;
  dev["cu"] = String("http://") + api::hostname() + ".local/";
  String out;
  serializeJson(d, out);
  client.publish((String("homeassistant/") + component + "/" + api::hostname() + "/" + id + "/config").c_str(),
                 out.c_str(), true);
}

void discovery() {
  const String state = base + "/state";
  struct Sensor {
    const char *id, *name, *tpl, *icon;
  };
  const Sensor sensors[] = {{"unseen", "Unseen alerts", "{{ value_json.unseen }}", "mdi:bell-badge"},
                            {"mood", "Mood", "{{ value_json.mood }}", "mdi:emoticon-outline"},
                            {"today", "Alerts today", "{{ value_json.today }}", "mdi:bell-ring"}};
  for (const Sensor &s : sensors) {
    JsonDocument d;
    d["stat_t"] = state;
    d["val_tpl"] = s.tpl;
    d["ic"] = s.icon;
    announce("sensor", s.id, s.name, d);
  }
  const Sensor binaries[] = {{"focus", "Focus", "{{ 'ON' if value_json.focus else 'OFF' }}", "mdi:timer-sand"},
                             {"present", "Someone at the desk", "{{ 'ON' if value_json.present else 'OFF' }}", ""}};
  for (const Sensor &s : binaries) {
    JsonDocument d;
    d["stat_t"] = state;
    d["val_tpl"] = s.tpl;
    if (*s.icon) d["ic"] = s.icon;
    else d["dev_cla"] = "occupancy";
    announce("binary_sensor", s.id, s.name, d);
  }
  const char *buttons[][3] = {{"poke", "Boop", "mdi:hand-pointing-up"},
                              {"focus", "Focus on/off", "mdi:timer-outline"},
                              {"demo", "Demo on/off", "mdi:play-circle-outline"}};
  for (auto &b : buttons) {
    JsonDocument d;
    d["cmd_t"] = base + "/cmd";
    d["pl_prs"] = b[0];
    d["ic"] = b[2];
    announce("button", b[0], b[1], d);
  }
  JsonDocument n;
  n["cmd_t"] = base + "/say";
  announce("notify", "say", "Ring Goli", n);
}

void publishState(const bot::State &s) {
  JsonDocument d;
  d["mood"] = s.mood;
  d["unseen"] = s.unseen;
  d["today"] = s.today_alerts;
  d["focus"] = s.focus;
  d["present"] = s.present;
  d["asleep"] = s.asleep;
  String out;
  serializeJson(d, out);
  client.publish((base + "/state").c_str(), out.c_str(), true);
}

void task(void *) {
  uint32_t seen_version = UINT32_MAX, last_try = 0, last_pub = 0;
  Settings cfg;
  bot::State last = {};
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(50));
    if (settings::changes() != seen_version) {  // (re)read the broker settings; reconnect only if they changed
      seen_version = settings::changes();
      const Settings next = settings::copy();
      if (next.mqtt_host != cfg.mqtt_host || next.mqtt_port != cfg.mqtt_port || next.mqtt_user != cfg.mqtt_user ||
          next.mqtt_pass != cfg.mqtt_pass) {
        if (client.connected()) {  // a clean disconnect doesn't send the last will: say goodbye ourselves
          client.publish((base + "/status").c_str(), "offline", true);
          client.disconnect();
        }
        last_try = 0;
      }
      cfg = next;
    }
    if (!cfg.mqtt_host.length()) {
      last_status = "off";
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      last_status = "waiting for Wi-Fi";
      continue;
    }
    if (!client.connected()) {
      if (last_try && millis() - last_try < 15000) continue;
      last_try = millis() | 1;
      base = String("goli/") + api::hostname();
      client.setServer(cfg.mqtt_host.c_str(), uint16_t(cfg.mqtt_port));
      client.setCallback(received);
      client.setBufferSize(1024);
      const String will = base + "/status";
      const bool ok = cfg.mqtt_user.length()
                          ? client.connect(api::hostname(), cfg.mqtt_user.c_str(), cfg.mqtt_pass.c_str(), will.c_str(), 0, true, "offline")
                          : client.connect(api::hostname(), will.c_str(), 0, true, "offline");
      if (!ok) {
        last_status = client.state() == MQTT_CONNECT_BAD_CREDENTIALS || client.state() == MQTT_CONNECT_UNAUTHORIZED
                          ? "wrong user or password"
                          : "broker unreachable";
        continue;
      }
      last_status = "connected";
      client.publish(will.c_str(), "online", true);
      discovery();
      client.subscribe((base + "/cmd").c_str());
      client.subscribe((base + "/say").c_str());
      client.subscribe((base + "/notify").c_str());
      last_pub = 0;
    }
    client.loop();
    bot::State now_state;
    bool have;
    portENTER_CRITICAL(&lock);
    now_state = shared;
    have = shared_set;
    portEXIT_CRITICAL(&lock);
    const bool changed = have && (strcmp(now_state.mood, last.mood ? last.mood : "") || now_state.unseen != last.unseen ||
                         now_state.focus != last.focus || now_state.present != last.present ||
                         now_state.today_alerts != last.today_alerts);
    if (have && (changed || millis() - last_pub > 60000)) {  // (`changed` reads mood only once `have`)
      publishState(now_state);
      last = now_state;
      last_pub = millis();
    }
  }
}

}  // namespace

void begin() { xTaskCreatePinnedToCore(task, "goli_mqtt", 6144, nullptr, 1, nullptr, 0); }

void setState(const bot::State &s) {
  portENTER_CRITICAL(&lock);
  shared = s;
  shared_set = true;
  portEXIT_CRITICAL(&lock);
}

const char *status() { return (const char *)last_status; }

}  // namespace mqtt
