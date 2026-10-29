#include "demo.h"

#include <Arduino.h>

#include "sound.h"
#include "text.h"

namespace demo {
namespace {

Face *f = nullptr;
bool on = false;
uint32_t started = 0;
int next = 0;

struct Step {
  uint32_t at_ms;
  void (*run)(Face &);
};

const Step kSteps[] = {
    {0, [](Face &x) { x.clearAlert(); x.setMood(Mood::TIRED); }},
    {3000, [](Face &x) {
       x.showAlert("staging: checkout 2.4.1 deployed", 1, AlertLevel::INFO);
       x.setMood(Mood::SURPRISED, 1200);
       sound::play(Sound::DING);
     }},
    {7000, [](Face &x) { x.clearAlert(); x.setMood(Mood::HAPPY, 2200); sound::play(Sound::CHIRP); }},
    {10500, [](Face &x) {
       x.showAlert("prod: payments-api drift detected", 1, AlertLevel::CRITICAL);
       x.setMood(Mood::ALARMED, 4000);
       sound::play(Sound::ALARM);
     }},
    {16000, [](Face &x) {
       x.clearAlert();
       x.showAlert("agent eu-west-1 is back", 1, AlertLevel::GOOD);
       x.setMood(Mood::HAPPY, 2500);
       sound::play(Sound::CHIRP);
     }},
    {20000, [](Face &x) { x.clearAlert(); x.setMood(Mood::DIZZY, 2500); sound::play(Sound::WOBBLE); }},
    {23500, [](Face &x) { x.setMood(Mood::WINK, 1300); sound::play(Sound::CHIRP); }},
    {26000, [](Face &x) { x.setMood(Mood::FOCUSED, 3000); }},
    {29500, [](Face &x) { x.setMood(Mood::DROWSY); }},
    {33000, [](Face &x) { x.setMood(Mood::ASLEEP); sound::play(Sound::YAWN); }},
    {39000, [](Face &x) { x.setMood(Mood::TIRED); x.setMood(Mood::SURPRISED, 700); sound::play(Sound::BOOP); }},
};
const int kCount = sizeof kSteps / sizeof *kSteps;
const uint32_t kLoopMs = 42000;

}  // namespace

void start(Face &face) {
  f = &face;
  on = true;
  started = millis();
  next = 0;
  face.showStatus(tr(Txt::DEMO_HINT));
}

void stop(Face &face, Mood resting) {
  on = false;
  face.clearAlert();  // the made-up ones; real alerts wait in the queue until the demo is over
  face.setMood(resting);
  face.showStatus(nullptr);
}

bool active() { return on; }

void update(uint32_t now) {
  if (!on) return;
  const uint32_t t = now - started;
  while (next < kCount && t >= kSteps[next].at_ms) kSteps[next++].run(*f);
  if (t >= kLoopMs) {
    started = now;
    next = 0;
  }
}

}  // namespace demo
