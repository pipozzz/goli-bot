#pragma once

#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "WString.h"  // host tests: test/shim
#endif

// iCalendar (.ics) reading for Goli's calendar, without any networking, so it runs in host tests too.
// Feed one calendar's lines, then finish() gives the meetings of the window, earliest first:
// daily and weekly repeats (BYDAY, INTERVAL, UNTIL, COUNT, EXDATE) are expanded, moved and cancelled
// instances (RECURRENCE-ID) replace their originals, VALARMs and free (TRANSP) events are ignored,
// and common time zones (TZID) are converted; others are read as Goli's own local time.
namespace ics {

const int kMeetings = 6;
struct Meeting {
  char title[48];
  int64_t start, end;  // Unix seconds; end = start when no length is given
};

void begin(int64_t from, int64_t to);  // a new window (from now until midnight); forgets everything
void line(String physical);            // one line as read (CRLF or LF stripped of the \n); folds are joined
void endOfFile();                      // after each calendar's last line
int finish(Meeting *out, int max);     // all calendars fed since begin(), applied and sorted

}  // namespace ics
