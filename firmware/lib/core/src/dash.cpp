#include "rigdash/dash.h"

#include "rigdash/alerts.h"
#include "rigdash/screen_manager.h"

namespace rigdash {

namespace {
uint16_t shiftStart(const Engine& e) { return static_cast<uint16_t>(e.shift_rpm * 9u / 10u); }
}  // namespace

uint8_t shiftLedsLit(const Engine& e, uint8_t n) {
  if (e.shift_rpm == 0 || n == 0) return 0;
  const uint16_t start = shiftStart(e);
  if (e.rpm <= start) return 0;
  if (e.rpm >= e.shift_rpm) return n;
  const uint32_t lit = static_cast<uint32_t>(e.rpm - start) * n / (e.shift_rpm - start) + 1;
  return static_cast<uint8_t>(lit > n ? n : lit);
}

bool shiftNow(const Engine& e) { return e.shift_rpm != 0 && e.rpm >= e.shift_rpm; }

int16_t deltaBarPx(int32_t delta_ms, int16_t half_width_px, int32_t range_ms) {
  if (range_ms <= 0) return 0;
  int64_t d = delta_ms;
  if (d > range_ms) d = range_ms;
  if (d < -range_ms) d = -range_ms;
  return static_cast<int16_t>(d * half_width_px / range_ms);
}

TempBand tyreBand(int16_t t, int16_t lo, int16_t hi) {
  if (lo == 0 && hi == 0) return TempBand::Unknown;
  if (t < lo) return TempBand::Cold;
  if (t > hi) return TempBand::Hot;
  return TempBand::Optimal;
}

TempBand brakeBand(int16_t c) {
  if (c < 150) return TempBand::Cold;
  if (c > 800) return TempBand::Hot;
  return TempBand::Optimal;
}

namespace {
Led flagLed(RaceFlag f) {
  switch (f) {
    case RaceFlag::Yellow:
    case RaceFlag::SafetyCar:
    case RaceFlag::VirtualSafetyCar: return Led::Yellow;
    case RaceFlag::Blue: return Led::Blue;
    case RaceFlag::Black:
    case RaceFlag::Red: return Led::Red;
    case RaceFlag::White:
    case RaceFlag::Chequered: return Led::White;
    default: return Led::Off;
  }
}
}  // namespace

Leds ledsFor(Screen screen, const State& s, uint32_t now) {
  Leds l;
  if (screen == Screen::NoSignal) return l;
  const bool blink = blinkOn(now);
  if (s.telemetry.has(kFlagPitLimiter)) {
    l.left = l.right = blink ? Led::Green : Led::Off;
    return l;
  }
  const Led flag = flagLed(s.telemetry.race_flag);
  if (flag != Led::Off) {
    l.left = l.right = blink ? flag : Led::Off;
    return l;
  }
  if (s.has_engine && s.engine.shift_rpm) {
    if (shiftNow(s.engine)) {
      // Fast flash at the shift point.
      l.left = l.right = blinkOn(now, 120) ? Led::Red : Led::Off;
      return l;
    }
    const uint8_t lit = shiftLedsLit(s.engine, 2);
    if (lit >= 1) l.left = Led::Green;
    if (lit >= 2) l.right = Led::Yellow;
    if (lit) return l;
  }
  if (screen != Screen::EngineOff) l.left = l.right = Led::Blue;
  return l;
}

Rgb alertColour(AlertKind k) {
  switch (k) {
    case AlertKind::RainSoon:
    case AlertKind::RainStarted:
    case AlertKind::RainIncreasing:
    case AlertKind::WetTyresAdvised: return rgb::kCyan;
    case AlertKind::TrackDrying:
    case AlertKind::PitWindowOpen: return rgb::kGreen;
    case AlertKind::LowFuel:
    case AlertKind::FuelShort:
    case AlertKind::Penalty: return rgb::kOrange;
    case AlertKind::Damage: return rgb::kRed;
    case AlertKind::TrackLimits: return rgb::kYellow;
    case AlertKind::Abs:
    case AlertKind::Tc:
    case AlertKind::TcCut:
    case AlertKind::EngineMap:
    case AlertKind::BrakeBias: return rgb::kBlue;
    case AlertKind::LastLap: return rgb::kWhite;
    case AlertKind::None: break;
  }
  return rgb::kOff;
}

namespace {
Rgb flagRgb(RaceFlag f) {
  switch (f) {
    case RaceFlag::Yellow:
    case RaceFlag::SafetyCar:
    case RaceFlag::VirtualSafetyCar: return rgb::kYellow;
    case RaceFlag::Blue: return rgb::kBlue;
    case RaceFlag::Black:
    case RaceFlag::Red: return rgb::kRed;
    case RaceFlag::White:
    case RaceFlag::Chequered: return rgb::kWhite;
    default: return rgb::kOff;
  }
}

void fillTop(Screen screen, const State& s, uint32_t now, uint8_t n, Rgb* top) {
  for (uint8_t i = 0; i < n; ++i) top[i] = rgb::kOff;
  if (n == 0 || screen == Screen::EngineOff) return;
  if (screen == Screen::NoSignal) {
    // Slow "waiting" scanner bouncing left and right.
    const uint32_t period = 2u * n;
    const uint32_t step = (now / 120u) % (period > 2 ? period - 2 : 1);
    const uint8_t pos = static_cast<uint8_t>(step < n ? step : period - 2 - step);
    top[pos < n ? pos : n - 1] = rgb::kDimBlue;
    return;
  }
  if (s.telemetry.has(kFlagPitLimiter)) {
    const bool phase = blinkOn(now);
    for (uint8_t i = 0; i < n; ++i) top[i] = ((i < n / 2) == phase) ? rgb::kBlue : rgb::kOff;
    return;
  }
  if (!s.has_engine) return;
  if (shiftNow(s.engine)) {
    if (blinkOn(now, 100)) {
      for (uint8_t i = 0; i < n; ++i) top[i] = rgb::kBlue;
    }
    return;
  }
  const uint8_t lit = shiftLedsLit(s.engine, n);
  for (uint8_t i = 0; i < lit; ++i) {
    const uint16_t pct = static_cast<uint16_t>((i + 1u) * 100u / n);
    top[i] = pct <= 50 ? rgb::kGreen : pct <= 80 ? rgb::kYellow : rgb::kRed;
  }
}

void fillSides(Screen screen, const State& s, AlertKind alert, uint32_t now, uint8_t n, Rgb* left,
               Rgb* right) {
  Rgb c = rgb::kOff;
  bool alternate = false;  // left/right (and neighbours) take turns
  const bool blink = blinkOn(now);
  const RaceFlag flag = s.telemetry.race_flag;
  if (screen == Screen::NoSignal || screen == Screen::EngineOff) {
    c = rgb::kOff;
  } else if (!flagRgb(flag).off()) {
    c = flagRgb(flag);
    alternate = flag == RaceFlag::Chequered || flag == RaceFlag::SafetyCar ||
                flag == RaceFlag::VirtualSafetyCar;
    if (!alternate && !blink) c = rgb::kOff;
  } else if (s.telemetry.has(kFlagPitLimiter)) {
    c = blink ? rgb::kGreen : rgb::kOff;
  } else if (screen == Screen::LapInvalidated) {
    c = rgb::kRed;
  } else if (alert != AlertKind::None) {
    c = blink ? alertColour(alert) : rgb::kOff;
  } else if (s.telemetry.best_lap_ms != 0) {
    c = s.telemetry.delta_ms <= 0 ? rgb::kDimGreen : rgb::kDimRed;
  } else {
    c = rgb::kDimBlue;
  }
  for (uint8_t i = 0; i < n; ++i) {
    if (alternate) {
      const bool on = ((i & 1) == 0) == blink;
      left[i] = on ? c : rgb::kOff;
      right[i] = on ? rgb::kOff : c;
    } else {
      left[i] = right[i] = c;
    }
  }
}
}  // namespace

void stripFor(Screen screen, const State& s, AlertKind alert, uint32_t now, const StripLayout& l,
              Rgb* out) {
  // Compute in logical order (top, left top→bottom, right top→bottom) and
  // scatter into chain order.
  constexpr uint8_t kMax = 64;
  Rgb top[kMax], left[kMax], right[kMax];
  const uint8_t nt = l.top < kMax ? l.top : kMax;
  const uint8_t ns = l.side < kMax ? l.side : kMax;
  fillTop(screen, s, now, nt, top);
  fillSides(screen, s, alert, now, ns, left, right);
  for (uint8_t i = 0; i < nt; ++i) out[l.topAt(i)] = top[i];
  for (uint8_t i = 0; i < ns; ++i) {
    out[l.left(i)] = left[i];
    out[l.right(i)] = right[i];
  }
}

}  // namespace rigdash
