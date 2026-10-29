#include "weather.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "face.h"
#include "settings.h"
#include "tls.h"

namespace weather {
namespace {

volatile uint8_t wear = 0;
volatile int temp_c = -999;  // bot::kNoTemp
char text[40] = "";
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;  // `text` is read by the main loop and the web page

void setText(const char *t) {
  portENTER_CRITICAL(&lock);
  snprintf(text, sizeof text, "%s", t);
  portEXIT_CRITICAL(&lock);
}

}  // namespace

void poll() {
  const Settings cfg = settings::copy();
  if (!cfg.lat.length() || !cfg.lon.length()) {
    wear = 0;
    temp_c = -999;
    setText("");
    return;
  }
  const String url = "https://api.open-meteo.com/v1/forecast?latitude=" + cfg.lat + "&longitude=" + cfg.lon +
                     "&current=temperature_2m,weather_code,is_day&hourly=precipitation_probability"
                     "&forecast_hours=8&timezone=auto";
  NetworkClientSecure tls;
  trustPublicRoots(tls);
  HTTPClient http;
  if (!http.begin(tls, url)) return;
  http.setTimeout(10000);
  http.useHTTP10(true);  // no chunked encoding under the JSON parser
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("weather: HTTP %d\n", code);
    http.end();
    return;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return;

  const float temp = doc["current"]["temperature_2m"] | 15.f;
  const int wmo = doc["current"]["weather_code"] | 0;  // WMO code: 0 clear, 1-3 clouds, 51+ drizzle/rain, 71+ snow
  const bool day = (doc["current"]["is_day"] | 1) == 1;
  int rain = 0;
  for (JsonVariant p : doc["hourly"]["precipitation_probability"].as<JsonArray>()) rain = max(rain, p.as<int>());

  uint8_t w = 0;
  const bool wet_now = (wmo >= 51 && wmo <= 67) || (wmo >= 80 && wmo <= 82) || wmo >= 95;
  if (wet_now || rain >= 60) w |= UMBRELLA;
  if (day && wmo <= 1 && temp >= 20.f && !(w & UMBRELLA)) w |= SUNGLASSES;
  if (temp <= 2.f || (wmo >= 71 && wmo <= 77) || wmo == 85 || wmo == 86) w |= SCARF;
  if (temp < 0.f) w |= COLD;
  if (temp > 28.f) w |= HOT;
  wear = w;
  temp_c = int(lroundf(temp));
  char t[40];
  snprintf(t, sizeof t, "%.0f C%s", temp, wet_now ? ", raining" : rain >= 60 ? ", rain later" : "");
  setText(t);
  Serial.printf("weather: %s (code %d, rain %d%%)\n", t, wmo, rain);
}

uint8_t accessories() { return wear; }
int temperature() { return temp_c; }

void summary(char *out, size_t cap) {
  portENTER_CRITICAL(&lock);
  snprintf(out, cap, "%s", text);
  portEXIT_CRITICAL(&lock);
}

}  // namespace weather
