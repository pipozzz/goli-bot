#include "reminders.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include <algorithm>

#include "text.h"

namespace reminders {
namespace {

const int kMax = 12;
Reminder all[kMax];
int count = 0;
Preferences store;
SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();

void lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGiveRecursive(mutex); }

static_assert(sizeof(Reminder) == 60, "the stored list's layout: change kVersion if it changes");
const int kVersion = 1;

void save() {  // under lock; one write, so a reset can't leave half a list
  if (count) store.putBytes("list", all, sizeof(Reminder) * count);
  else store.remove("list");
}

int dateOf(const struct tm &lt) { return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday; }

}  // namespace

void begin() {
  store.begin("goli-rem", false);
  lock();
  if (store.getInt("ver", 0) != kVersion) {  // another layout (or none): start empty
    store.clear();
    store.putInt("ver", kVersion);
  }
  const size_t bytes = store.getBytesLength("list");
  count = bytes % sizeof(Reminder) ? 0 : std::min<int>(kMax, bytes / sizeof(Reminder));
  if (count) store.getBytes("list", all, sizeof(Reminder) * count);
  unlock();
}

int list(Reminder *out, int max) {
  lock();
  const int n = count < max ? count : max;
  for (int i = 0; i < n; i++) out[i] = all[i];
  unlock();
  return n;
}

bool add(Reminder r) {
  lock();
  if (count >= kMax) {
    unlock();
    return false;
  }
  uint8_t id = 1;  // the smallest free id
  for (bool taken = true; taken; id++) {
    taken = false;
    for (int i = 0; i < count; i++) taken |= all[i].id == id;
    if (!taken) break;
  }
  r.id = id;
  r.fired = 0;
  char folded[sizeof r.title];
  foldAscii(r.title, folded, sizeof folded);  // the face shows ASCII
  memcpy(r.title, folded, sizeof r.title);
  all[count++] = r;
  save();
  unlock();
  return true;
}

bool remove(int id) {
  lock();
  bool found = false;
  for (int i = 0; i < count; i++) {
    if (all[i].id != id) continue;
    for (int j = i + 1; j < count; j++) all[j - 1] = all[j];
    count--;
    found = true;
    break;
  }
  if (found) save();
  unlock();
  return found;
}

bool due(char *title, size_t cap) {
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  struct tm lt;
  localtime_r(&now, &lt);
  const int today = dateOf(lt), minute = lt.tm_hour * 60 + lt.tm_min;
  bool rang = false, dropped = false;
  lock();
  for (int i = 0; i < count; i++) {  // one-time reminders whose day has passed (he was off)
    if (all[i].repeat != ONCE || all[i].date >= today) continue;
    for (int j = i + 1; j < count; j++) all[j - 1] = all[j];
    count--, i--;
    dropped = true;
  }
  if (dropped) save();
  for (int i = 0; i < count && !rang; i++) {
    Reminder &r = all[i];
    if (r.minute != minute || r.fired == today) continue;
    const bool weekday = lt.tm_wday >= 1 && lt.tm_wday <= 5;
    if ((r.repeat == ONCE && r.date != today) || (r.repeat == WEEKDAYS && !weekday)) continue;
    snprintf(title, cap, "%s", r.title);
    rang = true;
    if (r.repeat == ONCE) {
      for (int j = i + 1; j < count; j++) all[j - 1] = all[j];
      count--;
    } else {
      r.fired = today;
    }
    save();
  }
  unlock();
  return rang;
}

}  // namespace reminders
