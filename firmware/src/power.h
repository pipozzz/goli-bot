#pragma once

// Battery and USB power from the board's AXP2101.
namespace power {

struct State {
  bool battery = false;   // a battery is connected
  int percent = -1;       // -1 = unknown
  bool charging = false;
  bool usb = false;       // USB power present
  int millivolts = 0;
};

bool begin();  // after Wire.begin(); false if the PMU does not answer
State read();  // cheap enough to call every few seconds

}  // namespace power
