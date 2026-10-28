#pragma once

#include <stdint.h>

// Watches Goliash for new notifications (the same list as the bell in the Goliash UI).
// Wi-Fi and HTTP run in their own task so the face never stutters.
namespace goliash {

struct Alert {
  uint8_t count;      // new messages since the last poll
  char title[96];     // title of the newest one, folded to ASCII for the built-in font
};

enum class Status { OFF, CONNECTING, NO_WIFI, UNREACHABLE, BAD_TOKEN, OK };

bool configured();     // Wi-Fi and Goliash are set up
void begin();
Status status();
bool takeAlert(Alert &out);  // call from loop(); true when something new arrived
bool quietHours();     // no sounds at night (quiet hours from setup)

}  // namespace goliash
