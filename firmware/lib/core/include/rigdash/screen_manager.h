#pragma once
// Decides which screen is visible. Pure logic, no hardware — unit-tested.
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

enum class Screen : uint8_t {
  // User-selectable pages (cycled with the button). Order = Config::page_mask bits.
  CurrentLap = 0,
  Delta,      // live delta bar + sector times
  Tyres,      // temps, pressures, wear, brakes
  Car,        // fuel, ABS/TC/BB/map, damage
  Race,       // gaps, time/laps left, penalties, pit window
  Weather,    // rain now/forecast, wetness, grip, wind
  TrackData,  // track map, temperatures
  Laps,       // last/best/optimal + history
  // Overlays, chosen automatically:
  NoSignal = 0x80,
  RaceFlag,        // full-screen flag right after it changes
  LapInvalidated,
  Alert,           // popup from AlertEngine
  PitLimiter,
  EngineOff,
};

constexpr uint8_t kPageCount = 8;
constexpr uint32_t kSignalTimeoutMs = 2000;
constexpr uint32_t kInvalidOverlayMs = 2500;
constexpr uint32_t kFlagOverlayMs = 3000;
constexpr uint32_t kBlinkPeriodMs = 500;

// Shared blink phase for the pit limiter banner and LEDs: on for the first
// half of every period.
inline bool blinkOn(uint32_t now_ms, uint32_t period_ms = kBlinkPeriodMs) {
  return (now_ms % period_ms) < period_ms / 2;
}

inline bool isPage(Screen s) { return static_cast<uint8_t>(s) < kPageCount; }

class ScreenManager {
 public:
  void onTelemetry(const Telemetry& t, uint32_t now_ms);
  // Enabled pages from Config. If the current page gets disabled, the next
  // enabled one is selected. A mask without any page enables CurrentLap.
  void setPageMask(uint16_t mask);
  void nextPage();  // button press
  void prevPage();
  Screen current(uint32_t now_ms, bool alert_active = false) const;
  Screen page() const { return page_; }
  bool hasSignal(uint32_t now_ms) const;
  // Flag currently waved (for the flag strip / LEDs on every screen).
  rigdash::RaceFlag raceFlag() const { return last_.race_flag; }

 private:
  bool enabled(uint8_t i) const { return (mask_ >> i) & 1; }
  void step(int dir);

  Screen page_ = Screen::CurrentLap;
  uint16_t mask_ = (1u << kPageCount) - 1;
  Telemetry last_;
  bool have_data_ = false;
  uint32_t last_rx_ms_ = 0;
  uint32_t invalid_since_ms_ = 0;
  bool invalid_overlay_ = false;
  uint32_t flag_since_ms_ = 0;
  bool flag_overlay_ = false;
};

}  // namespace rigdash
