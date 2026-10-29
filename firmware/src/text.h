#pragma once

#include <stddef.h>
#include <stdint.h>

struct tm;

// The built-in fonts are ASCII only: fold common accented letters (Slovak, Czech, German...).
void foldAscii(const char *in, char *out, size_t cap);

// ---- Goli's words on the screen, in English, Slovak, Czech or German (ASCII: the fonts have no accents)
enum class Txt : uint8_t {
  ALERTS, TODAY, GOLIS_DAY, NOTHING_YET, NO_MEETINGS, NO_TIME, ROUGH_YESTERDAY, CALM_YESTERDAY,
  DAY_STATS,  // alerts, critical, good, focus sessions, pets, boops, yesterday
  HAPPY_BIRTHDAY, GOOD_MORNING, MEETINGS_1, MEETINGS_FEW, MEETINGS_MANY, RAIN_LATER, SUNNY, FREEZING,
  GO_HOME, STRETCH, ON_A_FRIDAY, SNOOZED, PAIR_CODE, IN_COUNTDOWN, DEMO_HINT,
  DRIFTING, TO_UPDATE, EXPLOITED, STALE, SERVICES_AGO, JUST_NOW, MIN_AGO, WAITING_GOLIASH, GOLIASH_NOT_SET,
  UPDATING, NO_WIFI, BAD_TOKEN, UNREACHABLE, REMINDER, IN_MEETING, BREAK, BACK_TO_WORK, TIMER, TIMES_UP, QUIET,
  COUNT
};

void setLanguage(const char *code);  // "en", "sk", "cs" or "de"; anything else is English
const char *language();
uint32_t languageVersion();          // changes when the language does (to redraw fixed labels)
const char *tr(Txt t);
const char *meetingsText(int n);     // "%d meetings" with the right plural
void dateText(const struct tm &lt, char *out, size_t cap);  // "Wed 29 Oct" / "St 29. okt"
