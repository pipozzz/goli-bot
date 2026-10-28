#include "face.h"

#include <Arduino.h>
#include <math.h>

#include <algorithm>
#include <string.h>

#include "config.h"

namespace {

// Goli's palette (same as the artwork)
const uint32_t kBlue = 0x6c7ae0;    // lens / lids
const uint32_t kLight = 0x8e99f0;   // brows, mouth (reads well on black)
const uint32_t kFrame = 0x3b4070;   // glasses frame
const uint32_t kInk = 0x2b2d42;     // pupils, lid line
const uint32_t kWhite = 0xffffff;
const uint32_t kCheek = 0xe5484d;
const uint32_t kGreen = 0x3fb27f;   // good news badge, charging

const int kC = LCD_SIZE / 2;   // screen centre
const int kEyeDx = 86, kEyeY = 220;
const int kRing = 70, kRingW = 13;
const int kLens = 58;          // lens radius (inside the frame)
const int kPupil = 26;
const int kLook = 20;          // max pupil travel, px
const int kBrowY = 120, kBrowHalf = 46;
const int kMouthY = 342, kMouthHalf = 40;

lv_obj_t *box(lv_obj_t *parent, int w, int h, uint32_t color, lv_coord_t radius = 0) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  return o;
}

lv_obj_t *line(lv_obj_t *parent, uint32_t color, int width) {
  lv_obj_t *l = lv_line_create(parent);
  lv_obj_remove_style_all(l);
  lv_obj_set_style_line_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_line_width(l, width, 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  return l;
}

float ease(float cur, float target, float dt_ms, float tau_ms) {
  return cur + (target - cur) * (1.f - expf(-dt_ms / tau_ms));
}

}  // namespace

void Face::begin(lv_obj_t *screen) {
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  root_ = lv_obj_create(screen);
  lv_obj_remove_style_all(root_);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(root_, LCD_SIZE, LCD_SIZE);

  for (int s = 0; s < 2; s++) {
    cheeks_[s] = box(root_, 56, 26, kCheek, LV_RADIUS_CIRCLE);
    lv_obj_set_pos(cheeks_[s], kC + (s ? 140 : -140) - 28, 312 - 13);
  }
  // Bridge of the glasses, behind the rings
  bridge_ = box(root_, 2 * (kEyeDx - kRing) + 12, 10, kFrame, 5);
  lv_obj_set_pos(bridge_, kC - (kEyeDx - kRing) - 6, kEyeY - 12);

  buildEye(eyes_[0], kC - kEyeDx, kEyeY, true);
  buildEye(eyes_[1], kC + kEyeDx, kEyeY, false);

  mouth_ = line(root_, kLight, 9);

  zzz_ = lv_label_create(root_);
  lv_label_set_text(zzz_, "z Z");
  lv_obj_set_style_text_font(zzz_, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(zzz_, lv_color_hex(kLight), 0);
  lv_obj_add_flag(zzz_, LV_OBJ_FLAG_HIDDEN);

  // Goliash alert: the red "!" badge from Goli's belly, with a count
  badge_ = box(root_, 64, 64, kCheek, LV_RADIUS_CIRCLE);
  lv_obj_set_style_border_color(badge_, lv_color_white(), 0);
  lv_obj_set_style_border_width(badge_, 4, 0);
  lv_obj_set_pos(badge_, kC - 32, 26);  // top centre, where the bell sits on his head
  badge_txt_ = lv_label_create(badge_);
  lv_obj_set_style_text_font(badge_txt_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(badge_txt_, lv_color_white(), 0);
  lv_obj_center(badge_txt_);
  lv_obj_add_flag(badge_, LV_OBJ_FLAG_HIDDEN);

  title_ = lv_label_create(root_);
  lv_obj_set_width(title_, 250);
  lv_label_set_long_mode(title_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title_, lv_color_white(), 0);
  lv_obj_set_pos(title_, kC - 125, 374);
  lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);

  status_ = lv_label_create(root_);
  lv_obj_set_width(status_, 240);
  lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(status_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(status_, lv_color_hex(0x9a9fb8), 0);
  lv_obj_set_pos(status_, kC - 120, 420);
  lv_obj_add_flag(status_, LV_OBJ_FLAG_HIDDEN);

  clock_ = lv_label_create(root_);
  lv_obj_set_width(clock_, 300);
  lv_obj_set_style_text_align(clock_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(clock_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(clock_, lv_color_hex(0x5a66c0), 0);  // soft: the screen is dimmed at night
  lv_obj_set_pos(clock_, kC - 150, 362);
  lv_obj_add_flag(clock_, LV_OBJ_FLAG_HIDDEN);

  focus_arc_ = lv_arc_create(root_);
  lv_obj_remove_style_all(focus_arc_);
  lv_obj_clear_flag(focus_arc_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(focus_arc_, LCD_SIZE - 12, LCD_SIZE - 12);
  lv_obj_set_pos(focus_arc_, 6, 6);
  lv_arc_set_rotation(focus_arc_, 270);
  lv_arc_set_bg_angles(focus_arc_, 0, 360);
  lv_arc_set_range(focus_arc_, 0, 1000);
  lv_obj_set_style_arc_width(focus_arc_, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(focus_arc_, lv_color_hex(kBlue), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(focus_arc_, true, LV_PART_INDICATOR);
  lv_obj_add_flag(focus_arc_, LV_OBJ_FLAG_HIDDEN);

  focus_txt_ = lv_label_create(root_);
  lv_obj_set_width(focus_txt_, 200);
  lv_obj_set_style_text_align(focus_txt_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(focus_txt_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(focus_txt_, lv_color_hex(kLight), 0);
  lv_obj_set_pos(focus_txt_, kC - 100, 392);
  lv_obj_add_flag(focus_txt_, LV_OBJ_FLAG_HIDDEN);

  battery_ = lv_label_create(root_);
  lv_obj_set_style_text_font(battery_, &lv_font_montserrat_28, 0);
  lv_obj_set_pos(battery_, kC + 52, 42);  // top, right of the alert badge
  lv_obj_add_flag(battery_, LV_OBJ_FLAG_HIDDEN);

  targetFor(mood_, tgt_);
  cur_ = tgt_;
  apply(0);
}

void Face::buildEye(Eye &e, int cx, int cy, bool left) {
  e.cx = cx;
  e.cy = cy;
  const int d = 2 * kLens;
  e.lens = box(root_, d, d, kWhite, LV_RADIUS_CIRCLE);
  lv_obj_set_style_clip_corner(e.lens, true, 0);  // children are clipped to the round lens
  lv_obj_set_pos(e.lens, cx - kLens, cy - kLens);

  e.pupil = box(e.lens, 2 * kPupil, 2 * kPupil, kInk, LV_RADIUS_CIRCLE);
  e.glint = box(e.lens, 16, 16, kWhite, LV_RADIUS_CIRCLE);
  e.lid_top = box(e.lens, d, d, kBlue);
  e.lid_line = box(e.lens, d, 9, kInk);
  e.lid_bottom = box(e.lens, d * 8 / 5, d, kBlue, LV_RADIUS_CIRCLE);

  e.ring = box(root_, 2 * kRing, 2 * kRing, 0, LV_RADIUS_CIRCLE);
  lv_obj_set_style_bg_opa(e.ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(e.ring, lv_color_hex(kFrame), 0);
  lv_obj_set_style_border_width(e.ring, kRingW, 0);
  lv_obj_set_pos(e.ring, cx - kRing, cy - kRing);

  e.brow = line(root_, kLight, 12);
  (void)left;
}

void Face::setBaseMood(Mood m) {
  base_ = m;
  if (!mood_until_) mood_ = m;
}

void Face::setMood(Mood m, uint32_t hold_ms) {
  mood_ = m;
  mood_until_ = hold_ms ? millis() + hold_ms : 0;
  if (!hold_ms) base_ = m;
}

void Face::blink() { blink_start_ = millis(); }

void Face::targetFor(Mood m, Shape &s) const {
  s = Shape();
  switch (m) {
    case Mood::TIRED:  // the classic Goli: "furt dačo"
      s.mouth = -1.f;
      break;
    case Mood::HAPPY:
      s.lid = 0.12f; s.lid_bottom = 0.42f; s.brow_tilt = 0.f; s.brow_lift = 8.f; s.mouth = 1.f; s.blush = 1.f;
      break;
    case Mood::SURPRISED:
      s.lid = 0.f; s.pupil = 0.65f; s.brow_tilt = 0.3f; s.brow_lift = 18.f; s.mouth = 2.f;
      break;
    case Mood::DIZZY:
      s.lid = 0.2f; s.pupil = 0.8f; s.brow_tilt = 1.4f; s.brow_lift = 6.f; s.mouth = -1.f;
      break;
    case Mood::DROWSY:
      s.lid = 0.72f; s.brow_tilt = 0.6f; s.brow_lift = -4.f; s.mouth = 0.f;
      break;
    case Mood::ASLEEP:
      s.lid = 1.f; s.brow_tilt = 0.2f; s.brow_lift = -6.f; s.mouth = 0.f;
      break;
    case Mood::WINK:
      s.lid = 0.1f; s.lid_bottom = 0.3f; s.brow_tilt = -0.2f; s.brow_lift = 6.f; s.mouth = 1.f; s.blush = 0.6f;
      break;
    case Mood::ALARMED:  // wide eyes, tiny pupils, brows up in a knot, mouth trembling
      s.lid = 0.f; s.pupil = 0.5f; s.brow_tilt = 2.2f; s.brow_lift = 20.f; s.mouth = -1.7f;
      break;
    case Mood::FOCUSED:  // steady eyes, brows down and in: working
      s.lid = 0.32f; s.pupil = 0.9f; s.brow_tilt = -0.7f; s.brow_lift = -2.f; s.mouth = 0.f;
      break;
    case Mood::WINCE:  // too loud: eyes squeezed, brows knotted
      s.lid = 0.5f; s.lid_bottom = 0.4f; s.pupil = 0.7f; s.brow_tilt = 1.8f; s.brow_lift = -8.f; s.mouth = -1.3f;
      break;
  }
  if (petting_) {  // eyes closed in bliss, big blush
    s.lid = 0.25f; s.lid_bottom = 0.55f; s.brow_tilt = 0.f; s.brow_lift = 6.f; s.mouth = 1.f; s.blush = 1.f;
  }
}

void Face::update(uint32_t now) {
  const float dt = last_ms_ ? float(now - last_ms_) : 16.f;
  last_ms_ = now;

  if (mood_until_ && now >= mood_until_) {
    mood_until_ = 0;
    mood_ = base_;
  }
  targetFor(mood_, tgt_);

  const float tau = 110.f;
  cur_.lid = ease(cur_.lid, tgt_.lid, dt, tau);
  cur_.lid_bottom = ease(cur_.lid_bottom, tgt_.lid_bottom, dt, tau);
  cur_.pupil = ease(cur_.pupil, tgt_.pupil, dt, 80.f);
  cur_.brow_lift = ease(cur_.brow_lift, tgt_.brow_lift, dt, tau);
  cur_.brow_tilt = ease(cur_.brow_tilt, tgt_.brow_tilt, dt, tau);
  cur_.mouth = ease(cur_.mouth, tgt_.mouth, dt, 140.f);
  cur_.blush = ease(cur_.blush, tgt_.blush, dt, 300.f);

  // Idle life: blinks and little glances around
  const bool awake = mood_ != Mood::ASLEEP;
  if (awake && now >= next_blink_) {
    blink_start_ = now;
    next_blink_ = now + (mood_ == Mood::DROWSY ? random(4000, 9000) : random(2200, 6500));
    if (random(6) == 0) next_blink_ = now + 260;  // double blink now and then
  }
  if (now >= next_saccade_) {
    saccade_x_ = random(-14, 15);
    saccade_y_ = random(-6, 9);
    next_saccade_ = now + random(900, 3800);
  }
  float tx = saccade_x_ + tilt_x_ * kLook, ty = saccade_y_ + tilt_y_ * kLook;
  if (mood_ == Mood::DIZZY) {
    tx = 16.f * cosf(now / 110.f);
    ty = 16.f * sinf(now / 110.f);
  } else if (mood_ == Mood::ALARMED) {  // jittery
    tx = random(-4, 5);
    ty = random(-3, 4);
  }
  look_x_ = ease(look_x_, constrain(tx, -kLook, kLook), dt, 45.f);
  look_y_ = ease(look_y_, constrain(ty, -kLook, kLook), dt, 45.f);

  apply(now);
}

void Face::apply(uint32_t now) {
  const int d = 2 * kLens;

  // Blink: a quick 150 ms close/open on top of the current lid
  float blink = 0.f;
  if (blink_start_ && now - blink_start_ < 150) {
    const float t = (now - blink_start_) / 150.f;
    blink = 1.f - fabsf(2.f * t - 1.f);
  }

  // Slow drift of the whole face: keeps the AMOLED from burning in one static image
  const int drift_x = int(6.f * sinf(now / 37000.f));
  const int breath = mood_ == Mood::ASLEEP ? int(4.f * sinf(now / 900.f)) : 0;
  lv_obj_set_pos(root_, drift_x, int(5.f * sinf(now / 53000.f)) + breath);

  for (int i = 0; i < 2; i++) {
    Eye &e = eyes_[i];
    float lid = cur_.lid;
    if (mood_ == Mood::WINK && i == 1) lid = 1.f;
    lid = fmaxf(lid, blink);
    // Dizzy: the two pupils spin in opposite directions
    const float lx = (mood_ == Mood::DIZZY && i == 1) ? -look_x_ : look_x_;
    const int pr = int(kPupil * cur_.pupil);
    const int px = kLens + int(lx) - pr;
    const int py = kLens + int(look_y_) + 8 - pr;  // pupils sit a little low: tired eyes
    lv_obj_set_size(e.pupil, 2 * pr, 2 * pr);
    lv_obj_set_pos(e.pupil, px, py);
    lv_obj_set_pos(e.glint, px + pr + pr / 3 - 8, py + pr / 4);

    const int lid_h = int(lid * d);
    lv_obj_set_height(e.lid_top, lid_h + 1);
    lv_obj_set_pos(e.lid_top, 0, 0);
    if (lid > 0.03f) {
      lv_obj_clear_flag(e.lid_line, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(e.lid_line, 0, std::min(lid_h - 5, d * 62 / 100));  // closed: lash line across the lens
    } else {
      lv_obj_add_flag(e.lid_line, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(e.lid_bottom, -d * 3 / 10, d - int(cur_.lid_bottom * d));

    // Brows: three points, inner end raised when worried
    const int dir = i == 0 ? 1 : -1;  // towards the nose
    const int y0 = kBrowY - int(cur_.brow_lift);
    const int inner = int(cur_.brow_tilt * 12.f);
    e.brow_pts[0] = {lv_coord_t(e.cx - dir * kBrowHalf), lv_coord_t(y0 + inner / 2)};
    e.brow_pts[1] = {lv_coord_t(e.cx), lv_coord_t(y0 - 10)};
    e.brow_pts[2] = {lv_coord_t(e.cx + dir * kBrowHalf), lv_coord_t(y0 - inner)};
    lv_line_set_points(e.brow, e.brow_pts, 3);
  }

  for (auto *c : cheeks_) lv_obj_set_style_bg_opa(c, lv_opa_t(110 + 140 * cur_.blush), 0);

  if (title_until_ && now >= title_until_) {
    title_until_ = 0;
    lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);
  }

  if (mood_ == Mood::ASLEEP && clock_on_) lv_obj_clear_flag(clock_, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(clock_, LV_OBJ_FLAG_HIDDEN);
  if (mood_ == Mood::ASLEEP) {
    lv_obj_clear_flag(zzz_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(zzz_, kC + 64, 46 + int(8.f * sinf(now / 700.f)));
  } else {
    lv_obj_add_flag(zzz_, LV_OBJ_FLAG_HIDDEN);
  }
  drawMouth(now);
}

void Face::showAlert(const char *title, int unseen, AlertLevel level) {
  level_ = unseen_ ? std::max(level_, level) : level;  // the worst unseen alert sets the colour
  unseen_ = unseen;
  static const uint32_t colors[] = {kGreen, kBlue, kCheek};
  lv_obj_set_style_bg_color(badge_, lv_color_hex(colors[int(level_)]), 0);
  lv_label_set_text_fmt(badge_txt_, unseen > 9 ? "9+" : "%d", unseen);
  lv_obj_clear_flag(badge_, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(title_, title);
  lv_obj_clear_flag(title_, LV_OBJ_FLAG_HIDDEN);
  title_until_ = millis() + 15000;
}

void Face::clearAlert() {
  unseen_ = 0;
  level_ = AlertLevel::GOOD;
  title_until_ = 0;
  lv_obj_add_flag(badge_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);
}

void Face::showStatus(const char *text) {
  if (!text) {
    lv_obj_add_flag(status_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (strcmp(lv_label_get_text(status_), text)) lv_label_set_text(status_, text);
  lv_obj_clear_flag(status_, LV_OBJ_FLAG_HIDDEN);
}

void Face::showBattery(int percent, bool charging) {
  if (percent < 0) {
    lv_obj_add_flag(battery_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const char *icon = charging        ? LV_SYMBOL_CHARGE " " LV_SYMBOL_BATTERY_3
                     : percent < 10  ? LV_SYMBOL_BATTERY_EMPTY
                                     : LV_SYMBOL_BATTERY_1;
  if (strcmp(lv_label_get_text(battery_), icon)) lv_label_set_text(battery_, icon);
  lv_obj_set_style_text_color(battery_, lv_color_hex(charging ? kGreen : kCheek), 0);
  lv_obj_clear_flag(battery_, LV_OBJ_FLAG_HIDDEN);
}

void Face::setClock(const char *hhmm) {
  clock_on_ = hhmm != nullptr;
  if (hhmm && strcmp(lv_label_get_text(clock_), hhmm)) lv_label_set_text(clock_, hhmm);
}

void Face::setFocus(int remaining_s, int total_s) {
  if (remaining_s < 0 || total_s <= 0) {
    lv_obj_add_flag(focus_arc_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(focus_txt_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_arc_set_value(focus_arc_, int16_t(1000L * remaining_s / total_s));
  lv_label_set_text_fmt(focus_txt_, "%d:%02d", remaining_s / 60, remaining_s % 60);
  lv_obj_clear_flag(focus_arc_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(focus_txt_, LV_OBJ_FLAG_HIDDEN);
}

void Face::drawMouth(uint32_t now) {
  const int n = 24;
  const float m = cur_.mouth;
  for (int k = 0; k < n; k++) {
    const float t = float(k) / (n - 1);
    const float u = 2.f * t - 1.f;
    float x = kC + u * kMouthHalf * (m > 1.f ? 1.f - 0.65f * (m - 1.f) : 1.f);
    float y;
    if (m < 0.f) {  // wavy, worried; the wave slowly crawls
      y = kMouthY + (-m) * (7.f * sinf(u * 4.2f + now / 600.f) - 4.f * u);
    } else if (m <= 1.f) {  // flat -> smile
      y = kMouthY + m * 20.f * (1.f - u * u) - m * 6.f;
    } else {  // smile -> small "o"
      const float a = 2.f * float(M_PI) * t;
      const float ox = kC + 15.f * sinf(a), oy = kMouthY + 4.f - 17.f * cosf(a);
      const float sy = kMouthY + 20.f * (1.f - u * u) - 6.f;
      const float w = fminf(m - 1.f, 1.f);
      x = x * (1.f - w) + ox * w;
      y = sy * (1.f - w) + oy * w;
    }
    mouth_pts_[k] = {lv_coord_t(x), lv_coord_t(y)};
  }
  lv_line_set_points(mouth_, mouth_pts_, n);
}
