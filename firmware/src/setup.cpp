#include "setup.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lvgl.h>

#include "config.h"
#include "settings.h"
#include "setup_page.h"
#include "setup_ui.h"
#include "sound.h"

namespace setup_mode {
namespace {

WebServer server(80);
DNSServer dns;
SetupUi ui;
uint32_t restart_at = 0;

String esc(const String &s) {
  String o;
  o.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}

void page() {
  const Settings &s = settings::get();
  String html = kSetupPage;
  html.replace("{{SSID}}", esc(s.ssid));
  html.replace("{{PASS_HINT}}", s.pass.length() ? "unchanged" : "");
  html.replace("{{URL}}", esc(s.url));
  html.replace("{{TOKEN_HINT}}", s.token.length() ? "unchanged" : "glsh_api_...");
  html.replace("{{CA}}", esc(s.ca));
  html.replace("{{VOL}}", String(s.volume));
  html.replace("{{QFROM}}", String(s.quiet_from));
  html.replace("{{QTO}}", String(s.quiet_to));
  server.send(200, "text/html; charset=utf-8", html);
}

void scan() {
  const int n = WiFi.scanNetworks();
  String json = "[";
  int listed = 0;
  for (int i = 0; i < n && listed < 20; i++) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length() || json.indexOf("\"" + ssid + "\"") >= 0) continue;  // hidden or duplicate
    String e = ssid;
    e.replace("\\", "\\\\");
    e.replace("\"", "\\\"");
    json += (listed++ ? ",\"" : "\"") + e + "\"";
  }
  WiFi.scanDelete();
  server.send(200, "application/json", json + "]");
}

void saveForm() {
  Settings &s = settings::get();
  s.ssid = server.arg("ssid");
  s.ssid.trim();
  if (server.arg("pass").length()) s.pass = server.arg("pass");  // empty = keep the old one
  s.url = server.arg("url");
  s.url.trim();
  while (s.url.endsWith("/")) s.url.remove(s.url.length() - 1);
  if (server.arg("token").length()) s.token = server.arg("token");
  s.token.trim();
  s.ca = server.arg("ca");
  s.ca.trim();
  s.volume = constrain(server.arg("vol").toInt(), 0, 100);
  s.quiet_from = constrain(server.arg("qfrom").toInt(), 0, 23);
  s.quiet_to = constrain(server.arg("qto").toInt(), 0, 23);
  settings::save();
  server.send(200, "text/html; charset=utf-8", kSavedPage);
  ui.showSaved();
  sound::play(Sound::CHIRP);
  restart_at = millis() + 2500;
}

// Phones probe these to detect a captive portal; answering with a redirect pops the page up.
void toPortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

}  // namespace

void run() {
  // Hotspot name from the MAC, a fresh random WPA2 password each time
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ssid[16], pass[12];
  snprintf(ssid, sizeof ssid, "Goli-%02X%02X", mac[4], mac[5]);
  snprintf(pass, sizeof pass, "%08lu", (unsigned long)(esp_random() % 100000000UL));

  WiFi.mode(WIFI_AP_STA);  // STA so the page can scan for networks
  WiFi.softAP(ssid, pass);
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, page);
  server.on("/scan", HTTP_GET, scan);
  server.on("/save", HTTP_POST, saveForm);
  server.onNotFound(toPortal);
  server.begin();

  ui.begin(lv_scr_act());
  ui.showJoin(ssid, pass);
  sound::play(Sound::CHIRP);
  Serial.printf("setup mode: join %s (password %s), open http://192.168.4.1\n", ssid, pass);

  bool joined = false;
  const uint32_t started = millis();
  uint32_t last = millis();
  for (;;) {
    const uint32_t now = millis();
    lv_tick_inc(now - last);
    last = now;
    dns.processNextRequest();
    server.handleClient();
    if (!joined && WiFi.softAPgetStationNum() > 0) {
      joined = true;
      ui.showOpen("http://192.168.4.1");
    }
    if (restart_at && now >= restart_at) ESP.restart();
    if (!restart_at && settings::hasWifi() && now - started > SETUP_TIMEOUT_MS) ESP.restart();
    lv_timer_handler();
    delay(5);
  }
}

}  // namespace setup_mode
