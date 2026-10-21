#include "rigdash/dash.h"

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

}  // namespace rigdash
