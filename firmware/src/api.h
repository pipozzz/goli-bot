#pragma once

#include <stdint.h>

// Goli's local API on your network (normal mode), at http://goli-xxxx.local/ or his IP:
//   POST /notify   ring him from anything: JSON {"title": "...", "level": "info|critical|good"},
//                  with "Authorization: Bearer <key>" (the key is on the setup page), or a Goliash
//                  webhook signed with the key as its secret
//   GET  /status   version, Wi-Fi and Goliash state (no secrets)
//   GET  /         the management page: pair a browser with a code Goli shows, then change
//                  everything from there (web_page.h, API under /api/)
namespace api {

// What the web page asks for that has to happen on the main loop (screen, sound volume, I2C)
enum class Command : uint8_t { POKE, FOCUS, DEMO, VOLUME, PAIR_CODE, SETUP };
struct Request {
  Command cmd;
  int arg;
};
bool take(Request &out);  // from the main loop

void begin();  // once, before Wi-Fi starts; serves from its own task when Wi-Fi is up
const char *hostname();  // goli-xxxx

}  // namespace api
