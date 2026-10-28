#pragma once

#include <stdint.h>

// The board's PCF85063 real-time clock keeps the time (UTC) while Goli is off, so his night
// clock is right even before Wi-Fi and NTP are up.
namespace rtc {

bool begin();               // after Wire.begin(): sets the system time from the RTC if it is valid
void maintain(uint32_t now);  // from the main loop: writes the time to the RTC after each NTP sync

}  // namespace rtc
