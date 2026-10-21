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

// "YELLOW FLAG", "SAFETY CAR", "CHEQUERED FLAG", ... ("" for None).
const char* raceFlagText(RaceFlag f);

// "26.1" / "-5.5" (degree sign is font dependent, renderer adds it).
// fahrenheit converts from °C first: "79.0".
char* formatTemp(int16_t temp_x10, char* out, size_t n, bool fahrenheit = false);

// "187" km/h or mph.
char* formatSpeed(uint16_t kmh, char* out, size_t n, bool mph = false);

// "27.5" psi or "1.90" bar.
char* formatPressure(uint16_t psi_x100, char* out, size_t n, bool bar = false);

// Gap to another car: "+1.2", "-0.8", "--.-" when unknown, "+1 L" style is
// left to the bridge (it sends ms). Clamped to "+999.9".
char* formatGap(int32_t ms, char* out, size_t n);

// Session time left: "45:12", "1:02:33", "--:--" when unknown.
char* formatDuration(uint32_t seconds, char* out, size_t n);

// "42.5 L".
char* formatLiters(uint16_t liters_x10, char* out, size_t n);

// "56.5%" brake bias, "--.-%" when unknown.
char* formatBias(uint16_t bias_x10, char* out, size_t n);

// "3" or "-" when the car has no such setting.
char* formatSetting(uint8_t value, char* out, size_t n);

const char* rainText(uint8_t level);        // "DRY" … "STORM"
const char* penaltyText(Penalty p);         // "DRIVE THROUGH", ""
const char* compoundText(Compound c);       // "SOFT", "WET", ""

}  // namespace rigdash
