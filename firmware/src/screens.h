#pragma once

#include <lvgl.h>

// The pages you swipe to from Goli's face: Goliash at a glance, a big clock, and how Goli is doing.
// They lie on top of the face; page FACE hides them all.
class Screens {
 public:
  enum Page { FACE = 0, OVERVIEW, CLOCK, STATUS, COUNT };

  void begin(lv_obj_t *screen);
  void show(int page);
  int page() const { return page_; }

  // Overview: workspace and the numbers that need someone. ago: "3 min ago". nullptr workspace =
  // no data yet, `ago` then says why.
  void setOverview(const char *workspace, int services, int drifting, int updates, int exploited, int stale,
                   const char *ago);
  void setClock(const char *hhmm, const char *date);
  void setStatus(const char *text);  // a few lines

 private:
  struct Stat {
    lv_obj_t *value, *label;
  };
  lv_obj_t *pages_[COUNT] = {}, *dots_[COUNT] = {}, *dot_row_ = nullptr;
  lv_obj_t *ws_ = nullptr, *ws_note_ = nullptr, *time_ = nullptr, *date_ = nullptr, *status_ = nullptr;
  Stat stats_[4] = {};
  int page_ = FACE;
};
