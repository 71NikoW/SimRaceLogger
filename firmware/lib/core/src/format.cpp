#include "rigdash/format.h"

#include <stdio.h>
#include <stdlib.h>

namespace rigdash {

char* formatLapTime(uint32_t ms, char* out, size_t n, int decimals) {
  if (decimals < 1) decimals = 1;
  if (decimals > 3) decimals = 3;
  if (ms == 0) {
    static const char* kEmpty[] = {"-:--.-", "-:--.--", "-:--.---"};
    snprintf(out, n, "%s", kEmpty[decimals - 1]);
    return out;
  }
  static const uint32_t kDiv[] = {100, 10, 1};
  const uint32_t minutes = ms / 60000;
  const uint32_t seconds = (ms / 1000) % 60;
  const uint32_t frac = (ms % 1000) / kDiv[decimals - 1];
  snprintf(out, n, "%lu:%02lu.%0*lu", static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds), decimals, static_cast<unsigned long>(frac));
  return out;
}

char* formatDelta(int32_t ms, char* out, size_t n) {
  int64_t v = ms;
  if (v > 99990) v = 99990;
  if (v < -99990) v = -99990;
  const char sign = v < 0 ? '-' : '+';
  const int64_t a = llabs(v);
  snprintf(out, n, "%c%lld.%02lld", sign, static_cast<long long>(a / 1000),
           static_cast<long long>((a % 1000) / 10));
  return out;
}

char* formatFuelLaps(uint16_t laps_x10, char* out, size_t n) {
  if (laps_x10 == 0xFFFF) {
    snprintf(out, n, "--.- LAPS");
  } else {
    snprintf(out, n, "%u.%u LAPS", laps_x10 / 10u, laps_x10 % 10u);
  }
  return out;
}

char* formatGear(int8_t gear, char* out, size_t n) {
  if (gear < 0) {
    snprintf(out, n, "R");
  } else if (gear == 0) {
    snprintf(out, n, "N");
  } else {
    snprintf(out, n, "%d", gear);
  }
  return out;
}

const char* raceFlagText(RaceFlag f) {
  switch (f) {
    case RaceFlag::Green: return "GREEN FLAG";
    case RaceFlag::Yellow: return "YELLOW FLAG";
    case RaceFlag::Blue: return "BLUE FLAG";
    case RaceFlag::White: return "WHITE FLAG";
    case RaceFlag::Black: return "BLACK FLAG";
    case RaceFlag::Chequered: return "CHEQUERED FLAG";
    case RaceFlag::Red: return "RED FLAG";
    case RaceFlag::SafetyCar: return "SAFETY CAR";
    case RaceFlag::VirtualSafetyCar: return "VIRTUAL SAFETY CAR";
    case RaceFlag::None: break;
  }
  return "";
}

char* formatTemp(int16_t temp_x10, char* out, size_t n) {
  const int a = abs(static_cast<int>(temp_x10));
  snprintf(out, n, "%s%d.%d", temp_x10 < 0 ? "-" : "", a / 10, a % 10);
  return out;
}

}  // namespace rigdash
