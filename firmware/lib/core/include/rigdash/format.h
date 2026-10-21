#pragma once
// Text formatting for the display. All functions write a NUL-terminated
// string into `out` (size `n`) and return `out`.
#include <stddef.h>
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

// "1:28.2" — truncates (never rounds up), like real timing systems.
// 0 means "no time" and renders as "-:--.-". decimals: 1..3.
char* formatLapTime(uint32_t ms, char* out, size_t n, int decimals = 1);

// "+0.35" / "-1.20" in seconds, clamped to "+99.99".
char* formatDelta(int32_t ms, char* out, size_t n);

// "12.3 LAPS" or "--.- LAPS" when unknown.
char* formatFuelLaps(uint16_t laps_x10, char* out, size_t n);

// "R", "N", "1".."9".
char* formatGear(int8_t gear, char* out, size_t n);

// "YELLOW FLAG", "BLUE FLAG", "CHEQUERED FLAG", ... ("" for None).
const char* raceFlagText(RaceFlag f);

// "26.1°" style without the degree sign (font dependent): "26.1", "-5.5".
char* formatTemp(int16_t temp_x10, char* out, size_t n);

}  // namespace rigdash
