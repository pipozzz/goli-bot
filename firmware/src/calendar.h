#pragma once

#include "bot.h"

// Today's meetings from a calendar's secret iCal address (Google, Outlook, iCloud, Nextcloud...).
// Only what's left of today is kept. Repeating events are expanded when they repeat daily or
// weekly (the usual stand-up); monthly and yearly ones are skipped.
namespace calendar {

void poll();                           // from the network task
int upcoming(bot::Meeting *out, int max);  // any task; earliest first
const char *state();                   // "off", "ok", or what went wrong

}  // namespace calendar
