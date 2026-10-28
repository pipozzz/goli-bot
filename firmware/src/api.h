#pragma once

// Goli's local API on your network (normal mode), at http://goli-xxxx.local/ or his IP:
//   POST /notify   ring him from anything: JSON {"title": "...", "level": "info|critical|good"},
//                  with "Authorization: Bearer <key>" (the key is on the setup page), or a Goliash
//                  webhook signed with the key as its secret
//   GET  /status   version, Wi-Fi and Goliash state (no secrets)
namespace api {

void begin();  // once, before Wi-Fi starts; serves from its own task when Wi-Fi is up
const char *hostname();  // goli-xxxx

}  // namespace api
