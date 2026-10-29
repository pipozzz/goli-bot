#pragma once

#include <stdint.h>

// How much an alert matters: an agent came back (GOOD), something changed (INFO),
// or something is broken or vulnerable (CRITICAL).
enum class AlertLevel : uint8_t { GOOD, INFO, CRITICAL };
