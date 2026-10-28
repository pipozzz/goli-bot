#include "rtc.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include "SensorPCF85063.hpp"
#include "config.h"

namespace rtc {
namespace {

SensorPCF85063 chip;
bool ok = false;

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm)
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return int64_t(era) * 146097 + int64_t(doe) - 719468;
}

bool clockSet() { return time(nullptr) > 1700000000; }

}  // namespace

bool begin() {
  setenv("TZ", GOLI_TZ, 1);
  tzset();
  ok = chip.begin(Wire, IIC_SDA, IIC_SCL);
  if (!ok || !chip.isClockIntegrityGuaranteed()) return ok;  // never set, or lost power
  const RTC_DateTime t = chip.getDateTime();
  if (t.getYear() < 2025) return ok;
  const timeval tv = {time_t(daysFromCivil(t.getYear(), t.getMonth(), t.getDay()) * 86400 + t.getHour() * 3600 +
                             t.getMinute() * 60 + t.getSecond()),
                      0};
  if (!clockSet()) settimeofday(&tv, nullptr);
  return ok;
}

void maintain(uint32_t now) {
  static uint32_t next = 0;
  if (!ok || int32_t(now - next) < 0) return;
  next = now + 10000;
  // Only time that came from NTP goes back into the RTC (the status reads COMPLETED once per sync)
  if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED || !clockSet()) return;
  const time_t t = time(nullptr);
  struct tm utc;
  gmtime_r(&t, &utc);
  chip.setDateTime(RTC_DateTime(utc));
}

}  // namespace rtc
