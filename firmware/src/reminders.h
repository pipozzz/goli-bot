#pragma once

#include <stddef.h>
#include <stdint.h>

// Reminders set on the web page: "14:00 call mum" once, every day or on weekdays. Kept in flash;
// Goli rings with the title at that minute (a missed minute, while he was off, is skipped).
namespace reminders {

enum Repeat : uint8_t { ONCE, DAILY, WEEKDAYS };

struct Reminder {
  uint8_t id;
  uint8_t repeat;
  int16_t minute;      // of the day, local time
  int32_t date;        // YYYYMMDD for ONCE
  int32_t fired;       // YYYYMMDD it last rang
  char title[48];
};

void begin();
int list(Reminder *out, int max);          // any task
bool add(Reminder r);                      // false when full (12)
bool remove(int id);
bool due(char *title, size_t cap);         // main loop, once a second: a reminder rings now

}  // namespace reminders
