#pragma once

#include <stdint.h>

#include "face.h"

// Demo mode for a booth: Goli plays through his faces and some made-up Goliash alerts on a loop,
// no Wi-Fi needed. Any tap ends it.
namespace demo {

void start(Face &face);
void stop(Face &face, Mood resting);  // back to his resting face (TIRED, or FOCUSED in focus mode)
bool active();
void update(uint32_t now);  // from the main loop while active

}  // namespace demo
