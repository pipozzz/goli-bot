#pragma once

#include <stdint.h>

// Golis on the same network know about each other (a UDP broadcast now and then). When one rings,
// the ones next to it look its way: set who sits left and right of each Goli on its web page.
namespace team {

void begin();                  // once Wi-Fi is up (from the network task)
void hello();                  // every minute (from the network task)
void rang(int level);          // this Goli rang: tell the others (any task)
bool takeGlance(int &dir);     // from the main loop: -1 look left, +1 look right

struct Peer {
  char host[16];
  uint32_t seen_ms;
};
int peers(Peer *out, int max);  // Golis heard from in the last 5 minutes

}  // namespace team
