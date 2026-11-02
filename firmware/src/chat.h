#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bot.h"

// Ask Goli something: tap, then hold him and talk. What the mics heard goes to a speech service
// (OpenAI-compatible: /audio/transcriptions, /audio/speech), the words to Claude, and the answer
// comes back as his voice and a line on his face. Off until both API keys are set on the web page.
namespace chat {

bool begin();      // after mic::begin(); false when there is no PSRAM for the audio
bool ready();      // both keys set: the gesture is worth offering
void listen();     // the finger stays on him: start recording
void send();       // the finger left: off to the cloud (in the chat task)

// What to show next: true once per change of phase. `text` stays valid until the next call.
bool poll(bot::Chat &phase, const char *&text, uint32_t &ms);

}  // namespace chat
