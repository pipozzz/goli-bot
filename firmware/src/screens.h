#pragma once

#include <lvgl.h>

// The pages you swipe to from Goli's face: Goliash at a glance, today's meetings, a big clock,
// Goli's day, and how Goli is doing.
// They lie on top of the face; page FACE hides them all.
class Screens {
 public:
  enum Page { FACE = 0, ALERTS, OVERVIEW, AGENDA, CLOCK, TODAY, STATUS, COUNT };

  void begin(lv_obj_t *screen);
  void show(int page);
  void step(int dir);                    // next (+1) or previous (-1) enabled page
  void enable(int page, bool on);        // e.g. no overview without Goliash
  int page() const { return page_; }

  // Overview: workspace and the numbers that need someone. ago: "3 min ago". nullptr workspace =
  // no data yet, `ago` then says why.
  void setOverview(const char *workspace, int services, int drifting, int updates, int exploited, int stale,
                   const char *ago);
  void setClock(const char *hhmm, const char *date);
  void setStatus(const char *text);  // a few lines
  void setAgenda(const char *text);  // today's meetings, a line each
  void setToday(const char *text);   // Goli's day in a few lines
  void setAlerts(const char *text);  // the latest alerts; #rrggbb ...# colours the times

 private:
  struct Stat {
    lv_obj_t *value, *label;
  };
  lv_obj_t *pages_[COUNT] = {}, *dots_[COUNT] = {}, *dot_row_ = nullptr;
  lv_obj_t *ws_ = nullptr, *ws_note_ = nullptr, *time_ = nullptr, *date_ = nullptr, *status_ = nullptr;
  lv_obj_t *agenda_ = nullptr, *today_ = nullptr, *alerts_ = nullptr;
  bool enabled_[COUNT] = {true, true, true, true, true, true, true};
  lv_obj_t *textPage(lv_obj_t *screen, Page p, const char *title);
  Stat stats_[4] = {};
  int page_ = FACE;
};
