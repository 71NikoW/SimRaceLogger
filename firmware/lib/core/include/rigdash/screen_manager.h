#pragma once
// Decides which screen is visible. Pure logic, no hardware — unit-tested.
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

enum class Screen : uint8_t {
  NoSignal,
  RaceFlag,  // full-screen flag right after it changes
  LapInvalidated,
  PitLimiter,
  EngineOff,
  // User-selectable pages (cycled with the button), in order:
  CurrentLap,
  TrackData,
  LapSummary,
};

constexpr uint32_t kSignalTimeoutMs = 2000;
constexpr uint32_t kInvalidOverlayMs = 2500;
constexpr uint32_t kFlagOverlayMs = 3000;
constexpr uint32_t kBlinkPeriodMs = 500;

// Shared blink phase for the pit limiter banner and LEDs: on for the first
// half of every period.
inline bool blinkOn(uint32_t now_ms, uint32_t period_ms = kBlinkPeriodMs) {
  return (now_ms % period_ms) < period_ms / 2;
}

class ScreenManager {
 public:
  void onTelemetry(const Telemetry& t, uint32_t now_ms);
  void nextPage();  // button press
  Screen current(uint32_t now_ms) const;
  Screen page() const { return page_; }
  // Flag currently waved (for the flag strip / LEDs on every screen).
  rigdash::RaceFlag raceFlag() const { return last_.race_flag; }
  bool hasSignal(uint32_t now_ms) const;

 private:
  Screen page_ = Screen::CurrentLap;
  Telemetry last_;
  bool have_data_ = false;
  uint32_t last_rx_ms_ = 0;
  uint32_t invalid_since_ms_ = 0;
  bool invalid_overlay_ = false;
  uint32_t flag_since_ms_ = 0;
  bool flag_overlay_ = false;
};

}  // namespace rigdash
