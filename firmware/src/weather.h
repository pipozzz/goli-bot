#pragma once

#include <stddef.h>
#include <stdint.h>

// What to wear, from Open-Meteo (free, no key): an umbrella when rain is likely in the next hours,
// sunglasses on a sunny warm afternoon, a scarf when it's freezing.
namespace weather {

void poll();             // from the network task
uint8_t accessories();   // Accessory bits for the face
void summary(char *out, size_t cap);  // "12 C, rain later" for the status page; "" when off

}  // namespace weather
