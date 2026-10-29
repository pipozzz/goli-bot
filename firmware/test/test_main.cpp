// Host tests for Goli's portable code: the iCalendar reader and the translations.
//   firmware/test/run.sh   (also in CI)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>

#include "ics.h"
#include "text.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                         \
  do {                                           \
    checks++;                                    \
    if (!(cond)) {                               \
      failures++;                                \
      printf("FAIL %s:%d: ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);                       \
      printf("\n");                              \
    }                                            \
  } while (0)

static int64_t utc(int y, int mo, int d, int h, int mi) {  // a UTC time as Unix seconds
  struct tm t = {};
  t.tm_year = y - 1900, t.tm_mon = mo - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = mi;
  return int64_t(timegm(&t));
}

// Today in the tests: Thursday 2026-10-29, 08:00 in Bratislava (07:00 UTC); window until midnight
static const int64_t kNow = utc(2026, 10, 29, 7, 0);
static const int64_t kMidnight = utc(2026, 10, 29, 22, 59);

static std::vector<ics::Meeting> parse(const char *text) {
  ics::begin(kNow - 60, kMidnight);
  std::string all(text);
  size_t from = 0;
  while (from < all.size()) {
    size_t nl = all.find('\n', from);
    if (nl == std::string::npos) nl = all.size();
    ics::line(String(all.substr(from, nl - from)));
    from = nl + 1;
  }
  ics::endOfFile();
  ics::Meeting m[ics::kMeetings];
  const int n = ics::finish(m, ics::kMeetings);
  return std::vector<ics::Meeting>(m, m + n);
}

static std::string event(const std::string &body) {
  return "BEGIN:VCALENDAR\nBEGIN:VEVENT\n" + body + "\nEND:VEVENT\nEND:VCALENDAR\n";
}

static bool has(const std::vector<ics::Meeting> &ms, const char *title, int64_t start) {
  for (auto &m : ms)
    if (!strcmp(m.title, title) && m.start == start) return true;
  return false;
}

static void testCalendar() {
  auto ms = parse(event("UID:a\nSUMMARY:Plain UTC\nDTSTART:20261029T100000Z\nDTEND:20261029T103000Z").c_str());
  CHECK(ms.size() == 1 && ms[0].start == utc(2026, 10, 29, 10, 0) && ms[0].end == utc(2026, 10, 29, 10, 30),
        "UTC event: %zu", ms.size());

  ms = parse(event("UID:b\nSUMMARY:Local\nDTSTART;TZID=Europe/Bratislava:20261029T093000").c_str());
  CHECK(has(ms, "Local", utc(2026, 10, 29, 8, 30)), "Europe/Bratislava after DST ends: 09:30 local = 08:30 UTC");

  ms = parse(event("UID:c\nSUMMARY:NY\nDTSTART;TZID=America/New_York:20261029T080000").c_str());
  CHECK(has(ms, "NY", utc(2026, 10, 29, 12, 0)), "New York still on EDT until Nov 1: 08:00 = 12:00 UTC");

  ms = parse(event("UID:d\nSUMMARY:London\nDTSTART;TZID=\"Europe/London\":20261029T120000").c_str());
  CHECK(has(ms, "London", utc(2026, 10, 29, 12, 0)), "quoted TZID, London on GMT");

  const std::string weekly = "UID:e\nSUMMARY:Stand-up\nDTSTART;TZID=Europe/Bratislava:20260901T090000\n"
                             "DURATION:PT15M\nRRULE:FREQ=WEEKLY;BYDAY=TU,TH";
  ms = parse(event(weekly).c_str());
  CHECK(ms.size() == 1 && has(ms, "Stand-up", utc(2026, 10, 29, 8, 0)) && ms[0].end == ms[0].start + 900,
        "weekly Tue/Thu stand-up today, 15 min");

  ms = parse(event(weekly + "\nEXDATE;TZID=Europe/Bratislava:20261029T090000").c_str());
  CHECK(ms.empty(), "EXDATE removes today's stand-up");

  ms = parse(("BEGIN:VCALENDAR\nBEGIN:VEVENT\nUID:f\nSUMMARY:Daily\nDTSTART:20261001T080000Z\nRRULE:FREQ=DAILY\n"
              "END:VEVENT\nBEGIN:VEVENT\nUID:f\nSUMMARY:Daily (moved)\nRECURRENCE-ID:20261029T080000Z\n"
              "DTSTART:20261029T130000Z\nEND:VEVENT\nEND:VCALENDAR\n"));
  CHECK(ms.size() == 1 && has(ms, "Daily (moved)", utc(2026, 10, 29, 13, 0)), "moved instance replaces the original");

  ms = parse(("BEGIN:VCALENDAR\nBEGIN:VEVENT\nUID:g\nSUMMARY:Cancelled one\nRECURRENCE-ID:20261029T080000Z\n"
              "STATUS:CANCELLED\nDTSTART:20261029T080000Z\nEND:VEVENT\nBEGIN:VEVENT\nUID:g\nSUMMARY:Series\n"
              "DTSTART:20261001T080000Z\nRRULE:FREQ=DAILY\nEND:VEVENT\nEND:VCALENDAR\n"));
  CHECK(ms.empty(), "cancelled instance (before its series in the file) removes today's");

  ms = parse(event("UID:h\nSUMMARY:Real title\nDTSTART:20261029T150000Z\nBEGIN:VALARM\nSUMMARY:Alarm notification\n"
                   "TRIGGER:-PT10M\nEND:VALARM").c_str());
  CHECK(ms.size() == 1 && !strcmp(ms[0].title, "Real title"), "VALARM doesn't rename it: '%s'", ms.empty() ? "" : ms[0].title);

  ms = parse(event("UID:i\nSUMMARY:Monthly\nDTSTART:20260929T090000Z\nRRULE:FREQ=MONTHLY").c_str());
  CHECK(ms.empty(), "monthly series from last month isn't expanded into today");

  ms = parse(event("UID:j\nSUMMARY:Busy? no\nTRANSP:TRANSPARENT\nDTSTART:20261029T100000Z").c_str());
  CHECK(ms.empty(), "free time is not a meeting");

  ms = parse(event("UID:k\nSUMMARY:A very long title that \n is folded\nDTSTART:20261029T100000Z").c_str());
  CHECK(ms.size() == 1 && !strcmp(ms[0].title, "A very long title that is folded"), "folded line: '%s'",
        ms.empty() ? "" : ms[0].title);

  ms = parse(event("UID:l\nSUMMARY:Ongoing\nDTSTART:20261029T063000Z\nDTEND:20261029T080000Z").c_str());
  CHECK(ms.size() == 1, "a meeting that is on right now is kept");

  ms = parse(event("UID:m\nSUMMARY:Holiday\nDTSTART;VALUE=DATE:20261029\nDTEND;VALUE=DATE:20261030").c_str());
  CHECK(ms.empty(), "all-day events aren't meetings");

  ms = parse(event("UID:n\nSUMMARY:Twice\nDTSTART:20261008T090000Z\nRRULE:FREQ=WEEKLY;COUNT=2").c_str());
  CHECK(ms.empty(), "weekly COUNT=2 from three weeks ago is over");

  ms = parse(event("UID:o\nSUMMARY:Until\nDTSTART:20261001T090000Z\nRRULE:FREQ=DAILY;UNTIL=20261020T000000Z").c_str());
  CHECK(ms.empty(), "daily UNTIL in the past is over");

  ms = parse(event("UID:p\nSUMMARY:Weekdays\nDTSTART:20261001T110000Z\nRRULE:FREQ=DAILY;BYDAY=MO,TU,WE,TH,FR").c_str());
  CHECK(ms.size() == 1, "daily on weekdays: Thursday counts");

  ms = parse(event("UID:q\nSUMMARY;LANGUAGE=sk:Porada \xc4\x8d. 2\nDTSTART:20261029T120000Z").c_str());
  CHECK(ms.size() == 1 && !strcmp(ms[0].title, "Porada c. 2"), "SUMMARY with parameters, accents folded: '%s'",
        ms.empty() ? "" : ms[0].title);

  ms = parse(event("UID:r\nSUMMARY:Biweekly\nDTSTART:20261015T100000Z\nRRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=TH").c_str());
  CHECK(ms.size() == 1, "every other Thursday: Oct 15 and Oct 29");

  ms = parse(event("UID:s\nSUMMARY:Off week\nDTSTART:20261022T100000Z\nRRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=TH").c_str());
  CHECK(ms.empty(), "every other Thursday from Oct 22: not today");

  // Earliest first, at most six
  std::string many = "BEGIN:VCALENDAR\n";
  for (int h = 22; h >= 9; h--) {
    char buf[160];
    snprintf(buf, sizeof buf, "BEGIN:VEVENT\nUID:t%d\nSUMMARY:At %d\nDTSTART:20261029T%02d0000Z\nEND:VEVENT\n", h, h, h - 1);
    many += buf;
  }
  ms = parse((many + "END:VCALENDAR\n").c_str());
  CHECK(ms.size() == 6 && !strcmp(ms[0].title, "At 9") && !strcmp(ms[5].title, "At 14"), "sorted, earliest six");

  ms = parse(event("UID:u\nSUMMARY:Ninety\nDTSTART:20261029T100000Z\nDURATION:PT1H30M").c_str());
  CHECK(ms.size() == 1 && ms[0].end - ms[0].start == 5400, "DURATION PT1H30M");
}

static int conversions(const char *f) {
  int n = 0;
  for (; *f; f++)
    if (*f == '%' && f[1] != '%') n++;
    else if (*f == '%') f++;
  return n;
}

static void testText() {
  const char *langs[] = {"en", "sk", "cs", "de"};
  for (int t = 0; t < int(Txt::COUNT); t++) {
    setLanguage("en");
    const int want = conversions(tr(Txt(t)));
    for (const char *l : langs) {
      setLanguage(l);
      CHECK(tr(Txt(t)) && *tr(Txt(t)), "text %d missing in %s", t, l);
      CHECK(conversions(tr(Txt(t))) == want, "text %d in %s has %d conversions, English %d", t, l,
            conversions(tr(Txt(t))), want);
      for (const char *c = tr(Txt(t)); *c; c++) CHECK((unsigned char)*c < 128, "text %d in %s isn't ASCII", t, l);
    }
  }
  setLanguage("sk");
  CHECK(!strcmp(meetingsText(1), "1 stretnutie") && strstr(meetingsText(3), "stretnutia") &&
            strstr(meetingsText(5), "stretnuti"),
        "Slovak plurals");
  struct tm lt = {};
  lt.tm_wday = 4, lt.tm_mday = 29, lt.tm_mon = 9;
  char d[32];
  dateText(lt, d, sizeof d);
  CHECK(!strcmp(d, "Stv 29. okt"), "Slovak date: '%s'", d);
  setLanguage("en");
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Goli's time zone (config.h)
  tzset();
  testCalendar();
  testText();
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
