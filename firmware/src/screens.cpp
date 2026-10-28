#include "screens.h"

#include <stdio.h>

#include "config.h"

LV_FONT_DECLARE(font_clock);  // big digits, tools/make_clock_font.py

namespace {

const uint32_t kLight = 0x8e99f0;
const uint32_t kMuted = 0x9a9fb8;
const uint32_t kRed = 0xe5484d;
const uint32_t kGreen = 0x3fb27f;
const int kC = LCD_SIZE / 2;

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int w) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, "");
  return l;
}

lv_obj_t *makePage(lv_obj_t *screen) {
  lv_obj_t *p = lv_obj_create(screen);
  lv_obj_remove_style_all(p);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(p, LCD_SIZE, LCD_SIZE);
  lv_obj_set_style_bg_color(p, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  return p;
}

}  // namespace

void Screens::begin(lv_obj_t *screen) {
  pages_[FACE] = nullptr;

  // Goliash at a glance: four numbers in a 2×2 grid
  lv_obj_t *o = pages_[OVERVIEW] = makePage(screen);
  ws_ = label(o, &lv_font_montserrat_28, kLight, 300);
  lv_obj_set_pos(ws_, kC - 150, 62);
  static const char *names[] = {"drifting", "to update", "exploited", "stale targets"};
  for (int i = 0; i < 4; i++) {
    const int x = kC + (i % 2 ? 22 : -182), y = 118 + (i / 2) * 120;
    stats_[i].value = label(o, &lv_font_montserrat_48, 0xffffff, 160);
    lv_obj_set_pos(stats_[i].value, x, y);
    stats_[i].label = label(o, &lv_font_montserrat_14, kMuted, 160);
    lv_obj_set_pos(stats_[i].label, x, y + 58);
    lv_label_set_text(stats_[i].label, names[i]);
  }
  ws_note_ = label(o, &lv_font_montserrat_14, kMuted, 300);
  lv_obj_set_pos(ws_note_, kC - 150, 372);

  // Big clock
  lv_obj_t *c = pages_[CLOCK] = makePage(screen);
  time_ = label(c, &font_clock, 0xffffff, 400);
  lv_obj_set_pos(time_, kC - 200, 136);
  date_ = label(c, &lv_font_montserrat_28, kLight, 300);
  lv_obj_set_pos(date_, kC - 150, 290);

  // How Goli is doing
  lv_obj_t *st = pages_[STATUS] = makePage(screen);
  lv_obj_t *head = label(st, &lv_font_montserrat_28, kLight, 300);
  lv_obj_set_pos(head, kC - 150, 70);
  lv_label_set_text(head, "Goli");
  status_ = label(st, &lv_font_montserrat_28, 0xffffff, 340);
  lv_obj_set_style_text_line_space(status_, 8, 0);
  lv_obj_set_pos(status_, kC - 170, 124);

  // Page dots
  dot_row_ = lv_obj_create(screen);
  lv_obj_remove_style_all(dot_row_);
  lv_obj_set_size(dot_row_, 16 * COUNT, 10);
  lv_obj_set_pos(dot_row_, kC - 8 * COUNT, 422);
  for (int i = 0; i < COUNT; i++) {
    dots_[i] = lv_obj_create(dot_row_);
    lv_obj_remove_style_all(dots_[i]);
    lv_obj_set_size(dots_[i], 8, 8);
    lv_obj_set_pos(dots_[i], 16 * i + 4, 1);
    lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dots_[i], LV_OPA_COVER, 0);
  }
  lv_obj_add_flag(dot_row_, LV_OBJ_FLAG_HIDDEN);
}

void Screens::show(int p) {
  page_ = ((p % COUNT) + COUNT) % COUNT;
  for (int i = 1; i < COUNT; i++) {
    if (i == page_) lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < COUNT; i++)
    lv_obj_set_style_bg_color(dots_[i], lv_color_hex(i == page_ ? 0xffffff : 0x3b4070), 0);
  if (page_ == FACE) lv_obj_add_flag(dot_row_, LV_OBJ_FLAG_HIDDEN);
  else {
    lv_obj_clear_flag(dot_row_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dot_row_);
  }
}

void Screens::setOverview(const char *workspace, int services, int drifting, int updates, int exploited, int stale,
                          const char *ago) {
  if (!workspace) {
    lv_label_set_text(ws_, "Goliash");
    for (auto &s : stats_) lv_label_set_text(s.value, "-");
    lv_label_set_text(ws_note_, ago);
    return;
  }
  lv_label_set_text(ws_, workspace);
  const int values[] = {drifting, updates, exploited, stale};
  const bool bad_when_any[] = {true, false, true, true};
  for (int i = 0; i < 4; i++) {
    lv_label_set_text_fmt(stats_[i].value, "%d", values[i]);
    const uint32_t color = values[i] == 0 ? kGreen : (bad_when_any[i] ? kRed : 0xffffff);
    lv_obj_set_style_text_color(stats_[i].value, lv_color_hex(color), 0);
  }
  lv_label_set_text_fmt(ws_note_, "%d services  -  %s", services, ago);
}

void Screens::setClock(const char *hhmm, const char *date) {
  lv_label_set_text(time_, hhmm);
  lv_label_set_text(date_, date);
}

void Screens::setStatus(const char *text) { lv_label_set_text(status_, text); }
