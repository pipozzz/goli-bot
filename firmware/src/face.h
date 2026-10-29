#pragma once

#include <lvgl.h>

// Goli's expressions. TIRED is his resting face: heavy lids, worried brows, wavy mouth.
enum class Mood { TIRED, HAPPY, SURPRISED, DIZZY, DROWSY, ASLEEP, WINK, ALARMED, FOCUSED, WINCE };

// How much a Goliash alert matters: an agent came back (GOOD), something changed (INFO),
// or something is broken or vulnerable (CRITICAL).
enum class AlertLevel : uint8_t { GOOD, INFO, CRITICAL };

// Things Goli wears: from the weather, the season and your birthday (bit mask)
enum Accessory : uint8_t {
  SUNGLASSES = 1, UMBRELLA = 2, SCARF = 4, PARTY_HAT = 8, SANTA_HAT = 16,
};

// Goli's animated face, drawn with plain LVGL objects on a 466×466 screen.
// Call update() every frame; it eases every feature toward the current mood.
class Face {
 public:
  void begin(lv_obj_t *screen);
  void update(uint32_t now_ms);

  void setMood(Mood mood, uint32_t hold_ms = 0);  // hold_ms > 0: return to the base mood afterwards
  void setBaseMood(Mood mood);
  Mood mood() const { return mood_; }
  bool held() const { return mood_until_ != 0; }  // a passing mood (setMood with hold_ms) is showing

  void blink();
  void setTilt(float x, float y) { tilt_x_ = x; tilt_y_ = y; }  // -1..1, from the IMU
  void setPetting(bool on) { petting_ = on; }

  // Goliash: a badge with the number of unseen messages (green / blue / red by level, the worst
  // unseen one wins), and the newest title scrolling under the mouth.
  void showAlert(const char *title, int unseen, AlertLevel level = AlertLevel::INFO);
  void clearAlert();
  void showStatus(const char *text);  // small grey hint at the bottom; nullptr hides it
  void showTitle(const char *text, uint32_t ms);  // big white line under the mouth for a while
  // Battery icon at the top, beside the badge: shown while charging or when low; percent < 0 hides it.
  void showBattery(int percent, bool charging);
  // Night clock under his closed eyes while he sleeps ("23:41"); nullptr hides it.
  void setClock(const char *hhmm);
  // Focus mode: a ring around the screen that shrinks, and the minutes left; remaining_s < 0 hides it.
  void setFocus(int remaining_s, int total_s, uint32_t color = 0x6c7ae0);  // blue: focus, green: a break
  // A meeting coming up: amber ring, its name and "in 4:59"; title nullptr hides it
  void setMeeting(const char *title, int remaining_s, int total_s);
  void setAccessories(uint8_t mask);
  void setFatigue(float f) { fatigue_ = f; }  // 0 fresh .. 1 exhausted: how heavy his resting lids are
  void glance(int dir, uint32_t ms);          // look hard to one side: -1 left, +1 right, 2 up
  void hop();                                 // a little jump of the whole face (an alert, good news)
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
  lv_obj_t *badge_ = nullptr, *badge_txt_ = nullptr, *title_ = nullptr, *status_ = nullptr, *battery_ = nullptr;
  lv_obj_t *clock_ = nullptr, *focus_arc_ = nullptr, *focus_txt_ = nullptr;
  bool clock_on_ = false, meeting_on_ = false;
  lv_obj_t *shades_[2] = {}, *umbrella_ = nullptr, *scarf_ = nullptr, *hat_ = nullptr, *drops_[3] = {};
  uint8_t hat_kind_ = 0, accessories_ = 0;
  float fatigue_ = 0.f;
  int glance_dir_ = 0;
  uint32_t glance_until_ = 0, hop_start_ = 0;
  void drawHat(uint8_t kind);
  int unseen_ = 0;
  AlertLevel level_ = AlertLevel::GOOD;
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
