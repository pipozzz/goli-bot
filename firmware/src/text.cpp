#include "text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void foldAscii(const char *in, char *out, size_t cap) {
  static const char *from[] = {"á", "ä", "à", "â", "č", "ď", "é", "ě", "è", "ê", "í", "ĺ", "ľ", "ň", "ó", "ô", "ö", "ŕ",
                               "ř", "š", "ť", "ú", "ů", "ü", "ý", "ž", "Á", "Ä", "Č", "Ď", "É", "Ě", "Í", "Ĺ", "Ľ", "Ň",
                               "Ó", "Ô", "Ö", "Ŕ", "Ř", "Š", "Ť", "Ú", "Ů", "Ü", "Ý", "Ž", "ß", "–", "—", "’", "“", "”", "…"};
  static const char *to[] = {"a", "a", "a", "a", "c", "d", "e", "e", "e", "e", "i", "l", "l", "n", "o", "o", "o", "r",
                             "r", "s", "t", "u", "u", "u", "y", "z", "A", "A", "C", "D", "E", "E", "I", "L", "L", "N",
                             "O", "O", "O", "R", "R", "S", "T", "U", "U", "U", "Y", "Z", "ss", "-", "-", "'", "\"", "\"", "..."};
  size_t o = 0;
  while (*in && o + 4 < cap) {
    if ((uint8_t)*in < 0x80) {
      out[o++] = *in++;
      continue;
    }
    bool hit = false;
    for (size_t k = 0; k < sizeof from / sizeof *from; k++) {
      const size_t len = strlen(from[k]);
      if (!strncmp(in, from[k], len)) {
        for (const char *t = to[k]; *t && o + 1 < cap;) out[o++] = *t++;
        in += len;
        hit = true;
        break;
      }
    }
    if (!hit) {  // unknown character: skip the whole UTF-8 sequence
      out[o++] = '?';
      for (in++; ((uint8_t)*in & 0xC0) == 0x80; in++) {
      }
    }
  }
  out[o] = 0;
}

// ---- languages

namespace {

const char *const kLangs[] = {"en", "sk", "cs", "de"};
const int kLangCount = 4;
volatile int lang = 0;
volatile uint32_t lang_version = 0;

const char *const kText[int(Txt::COUNT)][kLangCount] = {
    {"Alerts", "Upozornenia", "Upozorneni", "Meldungen"},  // ALERTS
    {"Today", "Dnes", "Dnes", "Heute"},  // TODAY
    {"Goli's day", "Goliho den", "Goliho den", "Golis Tag"},  // GOLIS_DAY
    {"nothing yet\ntoday", "dnes zatial\nnic", "dnes zatim\nnic", "heute noch\nnichts"},  // NOTHING_YET
    {"no more meetings\ntoday", "dnes uz ziadne\nstretnutia", "dnes uz zadne\nschuzky", "heute keine\nTermine mehr"},  // NO_MEETINGS
    {"no time yet", "cas zatial neviem", "cas zatim neznam", "Uhrzeit unbekannt"},  // NO_TIME
    {"rough yesterday", "vcera bolo rusno", "vcera bylo rusno", "gestern war viel los"},  // ROUGH_YESTERDAY
    {"calm yesterday", "vcera bol klud", "vcera byl klid", "gestern war ruhig"},  // CALM_YESTERDAY
    {"%d alerts, %d critical\n%d good news\n%d focus sessions\npetted %dx, booped %dx\n%s", "%d upozorneni, %d kritickych\n%d dobrych sprav\n%d sustredeni\npohladkany %dx, boop %dx\n%s", "%d upozorneni, %d kritickych\n%d dobrych zprav\n%d soustredeni\npohlazen %dx, boop %dx\n%s", "%d Meldungen, %d kritisch\n%d gute Nachrichten\n%d Fokus-Runden\ngestreichelt %dx, boop %dx\n%s"},  // DAY_STATS
    {"happy birthday!", "vsetko najlepsie!", "vse nejlepsi!", "alles Gute!"},  // HAPPY_BIRTHDAY
    {"good morning!", "dobre rano!", "dobre rano!", "guten Morgen!"},  // GOOD_MORNING
    {"1 meeting", "1 stretnutie", "1 schuzka", "1 Termin"},  // MEETINGS_1
    {"%d meetings", "%d stretnutia", "%d schuzky", "%d Termine"},  // MEETINGS_FEW
    {"%d meetings", "%d stretnuti", "%d schuzek", "%d Termine"},  // MEETINGS_MANY
    {"rain later", "neskor prsi", "pozdeji prsi", "spaeter Regen"},  // RAIN_LATER
    {"sunny", "slnecno", "slunecno", "sonnig"},  // SUNNY
    {"freezing", "mrzne", "mrzne", "Frost"},  // FREEZING
    {"it's Friday - go home!", "je piatok - chod domov!", "je patek - jdi domu!", "Freitag - ab nach Hause!"},  // GO_HOME
    {"stretch? water?", "natiahni sa? napi sa?", "protahni se? napij se?", "strecken? trinken?"},  // STRETCH
    {"on a Friday?!", "v piatok?!", "v patek?!", "am Freitag?!"},  // ON_A_FRIDAY
    {"snoozed for 10 min", "ticho na 10 min", "ticho na 10 min", "10 Min Ruhe"},  // SNOOZED
    {"code %06d", "kod %06d", "kod %06d", "Code %06d"},  // PAIR_CODE
    {"in %d:%02d", "o %d:%02d", "za %d:%02d", "in %d:%02d"},  // IN_COUNTDOWN
    {"demo - tap to stop", "demo - tukni a skonci", "demo - tukni pro konec", "Demo - tippen zum Ende"},  // DEMO_HINT
    {"drifting", "drift", "drift", "Drift"},  // DRIFTING
    {"to update", "aktualizovat", "aktualizovat", "zu aktualisieren"},  // TO_UPDATE
    {"exploited", "zneuzivane", "zneuzivane", "ausgenutzt"},  // EXPLOITED
    {"stale targets", "neaktivne ciele", "neaktivni cile", "inaktive Ziele"},  // STALE
    {"%d services  -  %s", "%d sluzieb  -  %s", "%d sluzeb  -  %s", "%d Dienste  -  %s"},  // SERVICES_AGO
    {"just now", "prave teraz", "prave ted", "gerade eben"},  // JUST_NOW
    {"%ld min ago", "pred %ld min", "pred %ld min", "vor %ld Min"},  // MIN_AGO
    {"waiting for Goliash...", "cakam na Goliash...", "cekam na Goliash...", "warte auf Goliash..."},  // WAITING_GOLIASH
    {"Goliash not set up (tap me 5x)", "Goliash nie je nastaveny (5x tukni)", "Goliash neni nastaven (5x tukni)", "Goliash fehlt (5x tippen)"},  // GOLIASH_NOT_SET
    {"updating myself...", "aktualizujem sa...", "aktualizuji se...", "aktualisiere mich..."},  // UPDATING
    {"no Wi-Fi - tap me 5x", "bez Wi-Fi - 5x tukni", "bez Wi-Fi - 5x tukni", "kein WLAN - 5x tippen"},  // NO_WIFI
    {"Goliash token? tap 5x", "Goliash token? 5x tukni", "Goliash token? 5x tukni", "Goliash-Token? 5x tippen"},  // BAD_TOKEN
    {"Goliash unreachable", "Goliash nedostupny", "Goliash nedostupny", "Goliash nicht erreichbar"},  // UNREACHABLE
    {"reminder", "pripomienka", "pripominka", "Erinnerung"},  // REMINDER
};

const char *const kDays[kLangCount][7] = {
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
    {"Ne", "Po", "Ut", "St", "Stv", "Pi", "So"},
    {"Ne", "Po", "Ut", "St", "Ct", "Pa", "So"},
    {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"},
};
const char *const kMonths[kLangCount][12] = {
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
    {"jan", "feb", "mar", "apr", "maj", "jun", "jul", "aug", "sep", "okt", "nov", "dec"},
    {"led", "uno", "bre", "dub", "kve", "cvn", "cvc", "srp", "zar", "rij", "lis", "pro"},
    {"Jan", "Feb", "Mrz", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"},
};

}  // namespace

void setLanguage(const char *code) {
  int next = 0;
  for (int i = 0; code && i < kLangCount; i++)
    if (!strcmp(code, kLangs[i])) next = i;
  if (next != lang) {
    lang = next;
    lang_version = lang_version + 1;
  }
}

const char *language() { return kLangs[lang]; }

uint32_t languageVersion() { return lang_version; }

const char *tr(Txt t) { return kText[int(t)][lang]; }

const char *meetingsText(int n) {
  if (n == 1) return tr(Txt::MEETINGS_1);
  return n >= 2 && n <= 4 ? tr(Txt::MEETINGS_FEW) : tr(Txt::MEETINGS_MANY);
}

void dateText(const struct tm &lt, char *out, size_t cap) {
  if (lang == 0) snprintf(out, cap, "%s %d %s", kDays[lang][lt.tm_wday], lt.tm_mday, kMonths[lang][lt.tm_mon]);
  else snprintf(out, cap, "%s %d. %s", kDays[lang][lt.tm_wday], lt.tm_mday, kMonths[lang][lt.tm_mon]);
}
