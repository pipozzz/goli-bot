#pragma once

#include <stdint.h>

// Turns raw accelerometer samples (in g) into what Goli cares about.
class Motion {
 public:
  // Call with each new sample; `now` in ms.
  void feed(float ax, float ay, float az, uint32_t now);

  float tiltX() const { return tilt_x_; }    // -1..1, lean left/right
  float tiltY() const { return tilt_y_; }    // -1..1, nod forward/back
  bool shaking() const { return shake_ > 1.2f; }
  bool takeJolt();                           // a single bump or pick-up, reported once
  bool lyingFlat() const { return flat_; }   // screen facing up or down
  uint32_t lastMoveMs() const { return last_move_; }

 private:
  bool calibrated_ = false;
  float g0x_ = 0, g0y_ = 0, g0z_ = 0;        // "upright" gravity, learned while still
  float lx_ = 0, ly_ = 0, lz_ = 1;           // low-passed gravity
  float tilt_x_ = 0, tilt_y_ = 0, shake_ = 0;
  bool flat_ = false, jolt_ = false;
  uint32_t still_since_ = 0, last_move_ = 0, last_ = 0;
};
