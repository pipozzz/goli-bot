#pragma once

#include <stdint.h>

// Watches Goliash for new notifications (the same list as the bell in the Goliash UI).
// Wi-Fi and HTTP run in their own task so the face never stutters.
namespace goliash {

struct Alert {
  uint8_t count;      // new messages since the last poll
  char title[96];     // title of the newest one, folded to ASCII for the built-in font
};

bool configured();     // false when firmware was built without include/secrets.h
void begin();
bool takeAlert(Alert &out);  // call from loop(); true when something new arrived
bool quietHours();     // no sounds at night (see QUIET_FROM / QUIET_TO)

}  // namespace goliash
