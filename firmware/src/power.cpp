#include "power.h"

#include <Arduino.h>
#include <Wire.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

#include "config.h"

namespace power {
namespace {

XPowersPMU pmu;
bool ok = false;

}  // namespace

bool begin() {
  ok = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!ok) return false;
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);  // nothing listens to its interrupt line
  pmu.clearIrqStatus();
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  return true;
}

State read() {
  State s;
  if (!ok) return s;
  s.usb = pmu.isVbusIn();
  s.battery = pmu.isBatteryConnect();
  if (s.battery) {
    s.percent = pmu.getBatteryPercent();
    s.charging = pmu.isCharging();
    s.millivolts = pmu.getBattVoltage();
  }
  return s;
}

}  // namespace power
