#pragma once

#include "bot.h"

// Home Assistant over MQTT: Goli announces himself (MQTT discovery) as a device with sensors
// (unseen alerts, mood, alerts today, focus, someone at the desk), buttons (boop, focus, demo) and a
// notify entity; anything published to goli/<name>/notify as {"title", "level"} rings him too.
namespace mqtt {

void begin();                  // starts its own task (call once a broker is set up; again is harmless)
void setState(const bot::State &s);  // from the main loop, every second
const char *status();          // "off", "connected", or what's wrong

}  // namespace mqtt
