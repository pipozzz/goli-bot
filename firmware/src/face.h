#pragma once

#include <lvgl.h>

// Goli's expressions. TIRED is his resting face: heavy lids, worried brows, wavy mouth.
enum class Mood { TIRED, HAPPY, SURPRISED, DIZZY, DROWSY, ASLEEP, WINK };

// Goli's animated face, drawn with plain LVGL objects on a 466×466 screen.
// Call update() every frame; it eases every feature toward the current mood.
class Face {
 public:
  void begin(lv_obj_t *screen);
  void update(uint32_t now_ms);

  void setMood(Mood mood, uint32_t hold_ms = 0);  // hold_ms > 0: return to the base mood afterwards
  void setBaseMood(Mood mood);
  Mood mood() const { return mood_; }

  void blink();
  void setTilt(float x, float y) { tilt_x_ = x; tilt_y_ = y; }  // -1..1, from the IMU
  void setPetting(bool on) { petting_ = on; }

  // Goliash: red badge with the number of unseen messages, and the newest title scrolling under the mouth.
  void showAlert(const char *title, int unseen);
  void clearAlert();
  int unseen() const { return unseen_; }

 private:
  struct Eye {
    lv_obj_t *lens, *pupil, *glint, *lid_top, *lid_line, *lid_bottom, *ring;
    lv_obj_t *brow;
    lv_point_t brow_pts[3];
    int cx, cy;
  };
  struct Shape {  // eased values
    float lid = 0.45f;        // top lid, 0 open .. 1 closed
    float lid_bottom = 0.f;   // smile lid from below, 0 .. 0.6
    float pupil = 1.f;        // pupil scale
    float brow_lift = 0.f;    // px, + is up
    float brow_tilt = 1.f;    // + worried (inner ends up), - cross
    float mouth = 0.f;        // -1 wavy/worried, 0 flat, +1 smile, 2 "o"
    float blush = 0.f;        // 0..1
  };

  void buildEye(Eye &e, int cx, int cy, bool left);
  void targetFor(Mood m, Shape &s) const;
  void apply(uint32_t now_ms);
  void drawMouth(uint32_t now_ms);

  lv_obj_t *root_ = nullptr;
  Eye eyes_[2];
  lv_obj_t *bridge_ = nullptr, *mouth_ = nullptr, *cheeks_[2] = {}, *zzz_ = nullptr;
  lv_obj_t *badge_ = nullptr, *badge_txt_ = nullptr, *title_ = nullptr;
  int unseen_ = 0;
  uint32_t title_until_ = 0;
  lv_point_t mouth_pts_[24];

  Mood mood_ = Mood::TIRED, base_ = Mood::TIRED;
  uint32_t mood_until_ = 0;
  Shape cur_, tgt_;
  float look_x_ = 0, look_y_ = 0, saccade_x_ = 0, saccade_y_ = 0;
  float tilt_x_ = 0, tilt_y_ = 0;
  bool petting_ = false;
  uint32_t blink_start_ = 0, next_blink_ = 2500, next_saccade_ = 1500, last_ms_ = 0;
};
