#pragma once
// Decides which screen is visible. Pure logic, no hardware — unit-tested.
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

enum class Screen : uint8_t {
  NoSignal,
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

class ScreenManager {
 public:
  void onTelemetry(const Telemetry& t, uint32_t now_ms);
  void nextPage();  // button press
  Screen current(uint32_t now_ms) const;
  Screen page() const { return page_; }
  bool hasSignal(uint32_t now_ms) const;

 private:
  Screen page_ = Screen::CurrentLap;
  Telemetry last_;
  bool have_data_ = false;
  uint32_t last_rx_ms_ = 0;
  uint32_t invalid_since_ms_ = 0;
  bool invalid_overlay_ = false;
};

}  // namespace rigdash
