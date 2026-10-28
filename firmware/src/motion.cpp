#include "motion.h"

#include <math.h>

#include "config.h"

void Motion::feed(float ax, float ay, float az, uint32_t now) {
  const float dt = last_ ? float(now - last_) : 20.f;
  last_ = now;

  // Gravity estimate (slow) and the "jerk" left over after removing it (fast)
  const float k = 1.f - expf(-dt / 250.f);
  lx_ += (ax - lx_) * k;
  ly_ += (ay - ly_) * k;
  lz_ += (az - lz_) * k;
  const float jx = ax - lx_, jy = ay - ly_, jz = az - lz_;
  const float jerk = sqrtf(jx * jx + jy * jy + jz * jz);

  // Shake meter: rises with jerk, decays in about a second
  shake_ = shake_ * expf(-dt / 600.f) + (jerk > 0.35f ? jerk * dt / 60.f : 0.f);
  if (jerk > 0.6f) jolt_ = true;
  if (jerk > 0.12f) {
    last_move_ = now;
    still_since_ = now;
  }

  const float g = sqrtf(lx_ * lx_ + ly_ * ly_ + lz_ * lz_);
  if (g < 0.5f) return;
  const float nx = lx_ / g, ny = ly_ / g, nz = lz_ / g;
  flat_ = fabsf(nz) > 0.9f;  // the screen normal is the IMU z axis

  // Learn "upright" once Goli has been standing still for 2 s (re-learned after big moves)
  if (!flat_ && now - still_since_ > 2000 && (!calibrated_ || jerk < 0.02f && now - still_since_ > 30000)) {
    g0x_ = nx; g0y_ = ny; g0z_ = nz;
    calibrated_ = true;
  }
  if (!calibrated_) return;

  // Roll = turn of gravity in the screen plane; pitch = gravity leaving the screen plane
  const float roll = atan2f(ny, nx) - atan2f(g0y_, g0x_);
  const float pitch = nz - g0z_;
  tilt_x_ = fmaxf(-1.f, fminf(1.f, TILT_ROLL_SIGN * sinf(roll) * 2.5f));
  tilt_y_ = fmaxf(-1.f, fminf(1.f, TILT_PITCH_SIGN * pitch * 2.5f));
}

bool Motion::takeJolt() {
  const bool j = jolt_;
  jolt_ = false;
  return j;
}
