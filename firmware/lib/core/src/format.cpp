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

char* formatTemp(int16_t temp_x10, char* out, size_t n, bool fahrenheit) {
  int v = temp_x10;
  if (fahrenheit) {
    // °F×10 = °C×10 × 9/5 + 320, rounded half away from zero.
    const int t = v * 9;
    v = (t >= 0 ? (t + 2) / 5 : (t - 2) / 5) + 320;
  }
  const int a = abs(v);
  snprintf(out, n, "%s%d.%d", v < 0 ? "-" : "", a / 10, a % 10);
  return out;
}

char* formatSpeed(uint16_t kmh, char* out, size_t n, bool mph) {
  const unsigned v = mph ? (kmh * 1000u + 804u) / 1609u : kmh;
  snprintf(out, n, "%u", v);
  return out;
}

char* formatPressure(uint16_t psi_x100, char* out, size_t n, bool bar) {
  if (bar) {
    // 1 psi = 0.0689476 bar; compute bar × 1000, then round to bar × 100.
    const unsigned b = static_cast<unsigned>((static_cast<uint64_t>(psi_x100) * 689476u / 1000000u + 5u) / 10u);
    snprintf(out, n, "%u.%02u", b / 100u, b % 100u);
  } else {
    const unsigned d = (psi_x100 + 5u) / 10u;
    snprintf(out, n, "%u.%u", d / 10u, d % 10u);
  }
  return out;
}

char* formatGap(int32_t ms, char* out, size_t n) {
  if (ms == kGapUnknown) {
    snprintf(out, n, "--.-");
    return out;
  }
  int64_t v = ms;
  if (v > 999900) v = 999900;
  if (v < -999900) v = -999900;
  const int64_t a = llabs(v);
  snprintf(out, n, "%c%lld.%lld", v < 0 ? '-' : '+', static_cast<long long>(a / 1000),
           static_cast<long long>((a % 1000) / 100));
  return out;
}

char* formatDuration(uint32_t s, char* out, size_t n) {
  if (s == kTimeLeftUnknown) {
    snprintf(out, n, "--:--");
  } else if (s >= 3600) {
    snprintf(out, n, "%lu:%02lu:%02lu", static_cast<unsigned long>(s / 3600),
             static_cast<unsigned long>((s / 60) % 60), static_cast<unsigned long>(s % 60));
  } else {
    snprintf(out, n, "%lu:%02lu", static_cast<unsigned long>(s / 60), static_cast<unsigned long>(s % 60));
  }
  return out;
}

char* formatLiters(uint16_t x10, char* out, size_t n) {
  snprintf(out, n, "%u.%u L", x10 / 10u, x10 % 10u);
  return out;
}

char* formatBias(uint16_t x10, char* out, size_t n) {
  if (x10 == 0) {
    snprintf(out, n, "--.-%%");
  } else {
    snprintf(out, n, "%u.%u%%", x10 / 10u, x10 % 10u);
  }
  return out;
}

char* formatSetting(uint8_t v, char* out, size_t n) {
  if (v == kSettingNa) {
    snprintf(out, n, "-");
  } else {
    snprintf(out, n, "%u", v);
  }
  return out;
}

const char* rainText(uint8_t level) {
  static const char* kText[] = {"DRY", "DRIZZLE", "LIGHT RAIN", "MEDIUM RAIN", "HEAVY RAIN", "STORM"};
  return kText[level > 5 ? 5 : level];
}

const char* penaltyText(Penalty p) {
  switch (p) {
    case Penalty::DriveThrough: return "DRIVE THROUGH";
    case Penalty::StopGo: return "STOP & GO";
    case Penalty::TimePenalty: return "TIME PENALTY";
    case Penalty::Disqualified: return "DISQUALIFIED";
    case Penalty::None: break;
  }
  return "";
}

const char* compoundText(Compound c) {
  switch (c) {
    case Compound::Soft: return "SOFT";
    case Compound::Medium: return "MEDIUM";
    case Compound::Hard: return "HARD";
    case Compound::Inter: return "INTER";
    case Compound::Wet: return "WET";
    case Compound::Unknown: break;
  }
  return "";
}

}  // namespace rigdash
