#include "ics.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#include "text.h"

namespace ics {
namespace {

// While parsing (one calendar day at a time)
struct Found {
  Meeting m;
  uint32_t uid;
};
const int kMax = kMeetings;
Found building[kMax];
int building_n = 0;
struct Moved {  // RECURRENCE-ID: this instance of a series was moved or cancelled
  uint32_t uid;
  int64_t original;
};
Moved moved[24];
int moved_n = 0;
int64_t win_from = 0, win_to = 0;  // today's window, Unix seconds

struct Event {
  char title[48];
  uint32_t uid;
  int64_t start;     // first occurrence, Unix seconds
  int64_t end;       // DTEND of the first occurrence (0 if none)
  int32_t duration;  // DURATION in seconds (0 if none)
  int64_t recurrence_id;
  bool all_day, cancelled;  // cancelled: also free time (TRANSP:TRANSPARENT)
  bool utc;          // DTSTART in UTC ("Z"): repeats keep the UTC time, not the local one
  // RRULE: daily and weekly are expanded; anything else counts as its first date only
  char freq;         // 0, 'D', 'W' or 'X'
  int interval, count;
  int64_t until;     // 0 = forever
  uint8_t byday;     // bit 0 = Sunday ... bit 6 = Saturday
  int64_t exdates[6];  // only those near today
  int ex_n;
};

uint32_t hash(const char *s) {  // FNV-1a, to tell series apart
  uint32_t h = 2166136261u;
  for (; *s; s++) h = (h ^ uint8_t(*s)) * 16777619u;
  return h;
}

int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return int64_t(era) * 146097 + int64_t(doe) - 719468;
}

// Day of month of the n-th (n > 0) or last (n = 0) Sunday of a month
int sunday(int y, int m, int n) {
  if (n) {
    const int wd = int((daysFromCivil(y, m, 1) + 4) % 7);  // 1970-01-01 was a Thursday
    return 1 + (7 - wd) % 7 + 7 * (n - 1);
  }
  const int last = m == 2 ? 28 : (m == 4 || m == 6 || m == 9 || m == 11) ? 30 : 31;
  const int wd = int((daysFromCivil(y, m, last) + 4) % 7);
  return last - wd;
}

// Time zones calendars use most, by IANA name: standard offset (minutes east of UTC) and DST rule.
// Goli has no zone database; others are read as Goli's own local time.
struct Zone {
  const char *prefix;
  int offset;
  char dst;  // 'E' EU rules, 'U' US rules, 0 none
};
const Zone kZones[] = {
    {"Europe/London", 0, 'E'},      {"Europe/Dublin", 0, 'E'},     {"Europe/Lisbon", 0, 'E'},
    {"Europe/Helsinki", 120, 'E'},  {"Europe/Kiev", 120, 'E'},     {"Europe/Kyiv", 120, 'E'},
    {"Europe/Athens", 120, 'E'},    {"Europe/Bucharest", 120, 'E'}, {"Europe/Sofia", 120, 'E'},
    {"Europe/Riga", 120, 'E'},      {"Europe/Vilnius", 120, 'E'},  {"Europe/Tallinn", 120, 'E'},
    {"Europe/", 60, 'E'},  // the rest of Europe is on CET
    {"America/New_York", -300, 'U'}, {"America/Toronto", -300, 'U'}, {"America/Chicago", -360, 'U'},
    {"America/Denver", -420, 'U'},  {"America/Phoenix", -420, 0},  {"America/Los_Angeles", -480, 'U'},
    {"UTC", 0, 0},                  {"Etc/UTC", 0, 0},             {"GMT", 0, 0},
    {"Asia/Tokyo", 540, 0},         {"Asia/Kolkata", 330, 0},      {"Asia/Singapore", 480, 0},
};

bool isDst(char rule, int y, int mo, int d, int h, int offset) {
  if (rule == 'E') {  // last Sunday of March 01:00 UTC .. last Sunday of October 01:00 UTC
    const int64_t t = daysFromCivil(y, mo, d) * 24 + h - offset / 60;  // hours, UTC (standard time)
    const int64_t on = daysFromCivil(y, 3, sunday(y, 3, 0)) * 24 + 1;
    const int64_t off = daysFromCivil(y, 10, sunday(y, 10, 0)) * 24 + 1;
    return t >= on && t < off;
  }
  if (rule == 'U') {  // second Sunday of March 02:00 .. first Sunday of November 02:00, local
    const int64_t t = daysFromCivil(y, mo, d) * 24 + h;
    return t >= daysFromCivil(y, 3, sunday(y, 3, 2)) * 24 + 2 && t < daysFromCivil(y, 11, sunday(y, 11, 1)) * 24 + 1;
  }
  return false;
}

// "20261029T093000Z" (UTC), "20261029T093000" with a TZID or local, or "20261029" (all day)
int64_t parseTime(const char *v, const char *tzid, bool *date_only) {
  int y, mo, d, h = 0, mi = 0, s = 0, n = 0;
  if (sscanf(v, "%4d%2d%2d%n", &y, &mo, &d, &n) != 3) return 0;
  *date_only = v[n] != 'T';
  if (!*date_only) sscanf(v + n + 1, "%2d%2d%2d", &h, &mi, &s);
  const int64_t wall = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;  // as if it were UTC
  if (strchr(v, 'Z')) return wall;
  if (tzid && *tzid) {
    for (const Zone &z : kZones) {
      if (strncmp(tzid, z.prefix, strlen(z.prefix))) continue;
      const int off = z.offset + (isDst(z.dst, y, mo, d, h, z.offset) ? 60 : 0);
      return wall - off * 60;
    }
  }
  struct tm tm = {};  // floating time, or a zone we don't know: Goli's own
  tm.tm_year = y - 1900, tm.tm_mon = mo - 1, tm.tm_mday = d, tm.tm_hour = h, tm.tm_min = mi, tm.tm_sec = s;
  tm.tm_isdst = -1;
  return int64_t(mktime(&tm));
}

void unescape(const char *in, char *out, size_t cap) {
  char raw[96];
  size_t o = 0;
  for (; *in && o + 1 < sizeof raw; in++) {
    if (*in == '\\' && in[1]) {
      in++;
      raw[o++] = *in == 'n' || *in == 'N' ? ' ' : *in;
    } else {
      raw[o++] = *in;
    }
  }
  raw[o] = 0;
  foldAscii(raw, out, cap);  // the fonts on the face are ASCII
}

void addOccurrence(const Event &e, int64_t start) {
  const int64_t length = e.duration ? e.duration : (e.end > e.start ? e.end - e.start : 0);
  if (start >= win_to || (start < win_from && start + length <= win_from)) return;  // ongoing ones count too
  if (start < win_from && length > 8 * 3600) return;  // a multi-day block already on: don't let it take a slot
  for (int i = 0; i < e.ex_n; i++)
    if (e.exdates[i] == start) return;
  int at = building_n;  // keep the earliest kMax, sorted
  while (at > 0 && building[at - 1].m.start > start) at--;
  if (at >= kMax) return;
  for (int i = std::min(building_n, kMax - 1); i > at; i--) building[i] = building[i - 1];
  snprintf(building[at].m.title, sizeof building[at].m.title, "%s", e.title);
  building[at].m.start = start;
  building[at].m.end = start + length;
  building[at].uid = e.uid;
  if (building_n < kMax) building_n++;
}

// Same time `days` later: the same UTC time for UTC events, else the same wall-clock time in Goli's
// time zone (DST-safe)
int64_t addDays(int64_t t, int days, bool utc) {
  if (utc) return t + int64_t(days) * 86400;
  time_t tt = time_t(t);
  struct tm lt;
  localtime_r(&tt, &lt);
  lt.tm_mday += days;
  lt.tm_isdst = -1;
  return int64_t(mktime(&lt));
}

int weekdayOf(int64_t t, bool utc) {
  time_t tt = time_t(t);
  struct tm lt;
  if (utc) gmtime_r(&tt, &lt);
  else localtime_r(&tt, &lt);
  return lt.tm_wday;
}

void expand(const Event &e) {
  if (e.recurrence_id) {  // a moved (or cancelled) instance: its series' original time goes away
    if (moved_n < 24) moved[moved_n++] = {e.uid, e.recurrence_id};
    if (!e.all_day && !e.cancelled && e.start) addOccurrence(e, e.start);
    return;
  }
  if (e.all_day || e.cancelled || !e.start) return;
  if (!e.freq || e.freq == 'X') return addOccurrence(e, e.start);
  const int step = std::max(1, e.interval);
  const int64_t period = 86400LL * (e.freq == 'W' ? 7 : 1) * step;
  // With COUNT, walk from the first occurrence so the count is exact; otherwise jump near today
  int64_t k = 0;
  if (!e.count && win_from > e.start) k = std::max<int64_t>(0, (win_from - e.start) / period - 1);
  int seen = 0;
  for (int guard = 0; guard < 4000; guard++, k++) {
    if (e.freq == 'D') {
      const int64_t t = addDays(e.start, int(k * step), e.utc);
      if (t >= win_to || (e.until && t > e.until)) return;
      if (e.byday && !(e.byday & (1 << weekdayOf(t, e.utc)))) continue;  // FREQ=DAILY;BYDAY=MO,...,FR
      if (e.count && ++seen > e.count) return;
      addOccurrence(e, t);
      continue;
    }
    // Weekly: weeks start on Monday (RFC 5545's default WKST)
    const int64_t first_monday = addDays(e.start, -((weekdayOf(e.start, e.utc) + 6) % 7), e.utc);
    const int64_t monday = addDays(first_monday, int(k * 7 * step), e.utc);
    if (monday >= win_to) return;
    const uint8_t days = e.byday ? e.byday : uint8_t(1 << weekdayOf(e.start, e.utc));
    for (int i = 0; i < 7; i++) {  // Monday .. Sunday
      if (!(days & (1 << ((i + 1) % 7)))) continue;
      const int64_t t = addDays(monday, i, e.utc);
      if (t < e.start) continue;
      if (e.until && t > e.until) return;
      if (e.count && ++seen > e.count) return;
      addOccurrence(e, t);
    }
  }
}

bool named(const String &name, const char *prop) {  // "SUMMARY" or "SUMMARY;LANGUAGE=en-US"
  const size_t n = strlen(prop);
  return name.startsWith(prop) && (name.length() == n || name[n] == ';');
}

String param(const String &name, const char *key) {  // "DTSTART;TZID=Europe/Bratislava" -> "Europe/Bratislava"
  int i = name.indexOf(key);
  if (i < 0) return String();
  i += strlen(key);
  int end = name.indexOf(';', i);
  String v = name.substring(i, end < 0 ? name.length() : end);
  v.replace("\"", "");
  return v;
}

void property(Event &e, const String &line) {
  const int colon = line.indexOf(':');
  if (colon < 0) return;
  const String name = line.substring(0, colon);
  const String value = line.substring(colon + 1);
  bool date_only = false;
  if (named(name, "SUMMARY")) {
    unescape(value.c_str(), e.title, sizeof e.title);
  } else if (named(name, "UID")) {
    e.uid = hash(value.c_str());
  } else if (named(name, "DTSTART")) {
    e.start = parseTime(value.c_str(), param(name, "TZID=").c_str(), &date_only);
    e.all_day = date_only;
    e.utc = value.endsWith("Z");
  } else if (named(name, "DTEND")) {
    e.end = parseTime(value.c_str(), param(name, "TZID=").c_str(), &date_only);
  } else if (named(name, "DURATION")) {  // "PT1H30M", "PT45M", "P1DT2H"
    int n = 0, sum = 0;
    for (const char *p = value.c_str(); *p; p++) {
      if (isdigit(*p)) n = n * 10 + (*p - '0');
      else {
        sum += *p == 'W' ? n * 604800 : *p == 'D' ? n * 86400 : *p == 'H' ? n * 3600 : *p == 'M' ? n * 60 : *p == 'S' ? n : 0;
        n = 0;
      }
    }
    e.duration = value.startsWith("-") ? 0 : sum;  // a negative length isn't one
  } else if (named(name, "RECURRENCE-ID")) {
    e.recurrence_id = parseTime(value.c_str(), param(name, "TZID=").c_str(), &date_only);
  } else if (named(name, "STATUS")) {
    if (value == "CANCELLED") e.cancelled = true;
  } else if (named(name, "TRANSP")) {
    if (value == "TRANSPARENT") e.cancelled = true;  // marked as free: not a meeting
  } else if (named(name, "EXDATE")) {
    const String tz = param(name, "TZID=");
    for (int from = 0; from < int(value.length());) {
      int comma = value.indexOf(',', from);
      if (comma < 0) comma = value.length();
      const int64_t t = parseTime(value.substring(from, comma).c_str(), tz.c_str(), &date_only);
      if (t >= win_from - 86400 && t < win_to && e.ex_n < 6) e.exdates[e.ex_n++] = t;  // only today's matter
      from = comma + 1;
    }
  } else if (named(name, "RRULE")) {
    e.interval = 1;
    if (value.indexOf("FREQ=DAILY") >= 0) e.freq = 'D';
    else if (value.indexOf("FREQ=WEEKLY") >= 0) e.freq = 'W';
    else e.freq = 'X';  // monthly, yearly...: not expanded
    int i = value.indexOf("INTERVAL=");
    if (i >= 0) e.interval = value.substring(i + 9).toInt();
    i = value.indexOf("COUNT=");
    if (i >= 0) e.count = value.substring(i + 6).toInt();
    i = value.indexOf("UNTIL=");
    if (i >= 0) {
      e.until = parseTime(value.substring(i + 6).c_str(), nullptr, &date_only);
      if (date_only) e.until += 86399;
    }
    i = value.indexOf("BYDAY=");
    if (i >= 0) {
      static const char *names[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
      const int end = value.indexOf(';', i);
      const String days = value.substring(i + 6, end < 0 ? value.length() : end);
      for (int d = 0; d < 7; d++)
        if (days.indexOf(names[d]) >= 0) e.byday |= 1 << d;
    }
  }
}



String pending;  // the logical line being unfolded
int depth = 0;   // 1 inside a VEVENT, 2 inside its VALARM
Event e = {};

void handle(const String &l) {
  if (l == "BEGIN:VEVENT") {
    e = {};
    depth = 1;
  } else if (l == "END:VEVENT") {
    if (depth) expand(e);
    depth = 0;
  } else if (depth && l.startsWith("BEGIN:")) {
    depth++;  // VALARM: its SUMMARY is the alarm's, not the meeting's
  } else if (depth > 1 && l.startsWith("END:")) {
    depth--;
  } else if (depth == 1) {
    property(e, l);
  }
}

}  // namespace

void begin(int64_t from, int64_t to) {
  win_from = from;
  win_to = to;
  building_n = moved_n = 0;
  pending = "";
  depth = 0;
}

void line(String l) {
  if (l.endsWith("\r")) l.remove(l.length() - 1);
  if (l.length() && (l[0] == ' ' || l[0] == '\t')) {  // a folded line continues the last one
    if (pending.length() < 512) pending += l.substring(1);  // capped: some carry whole HTML bodies
    return;
  }
  if (pending.length()) handle(pending);
  pending = l;
}

void endOfFile() {
  if (pending.length()) handle(pending);
  pending = "";
  depth = 0;
}

int finish(Meeting *out, int max) {
  // Moved or cancelled instances replace their series' original occurrence
  for (int m = 0; m < moved_n; m++) {
    for (int i = 0; i < building_n; i++) {
      if (building[i].uid != moved[m].uid || building[i].m.start != moved[m].original) continue;
      for (int j = i + 1; j < building_n; j++) building[j - 1] = building[j];
      building_n--;
      break;
    }
  }
  const int n = std::min(building_n, max);
  for (int i = 0; i < n; i++) out[i] = building[i].m;
  return n;
}

}  // namespace ics
