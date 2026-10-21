#pragma once
// Popups the display shows for a few seconds when something changes:
// weather warnings, low fuel, damage, penalties, track limits, the driver
// changing ABS/TC/brake bias on the wheel, last lap, pit window.
// Edge-triggered: each alert fires once per change, not continuously.
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

enum class AlertKind : uint8_t {
  None,
  RainSoon,        // value = forecast level in 10 min
  RainStarted,     // value = current level
  RainIncreasing,  // value = current level
  TrackDrying,     // value = wetness
  WetTyresAdvised,
  LowFuel,         // value = laps × 10
  FuelShort,       // value = liters × 10 missing to finish
  Damage,          // value = damage %
  Penalty,         // value = Penalty, extra = seconds
  TrackLimits,     // value = count, extra = max
  Abs,             // value = new setting
  Tc,
  TcCut,
  EngineMap,
  BrakeBias,       // value = bias × 10
  LastLap,
  PitWindowOpen,
};

struct Alert {
  AlertKind kind = AlertKind::None;
  int32_t value = 0;
  int32_t extra = 0;
  uint32_t since_ms = 0;
};

constexpr uint32_t kAlertMs = 3000;        // normal popups
constexpr uint32_t kSettingAlertMs = 1500; // ABS/TC/BB popups, short
constexpr uint16_t kLowFuelLapsX10 = 20;   // warn below 2 laps
constexpr uint8_t kDamageStepPct = 5;

class AlertEngine {
 public:
  // Call after every applied frame (or once per loop) with the full state.
  void update(const State& s, uint32_t now_ms);
  // Alert to show now, or kind None.
  Alert current(uint32_t now_ms) const;
  bool active(uint32_t now_ms) const { return current(now_ms).kind != AlertKind::None; }
  uint8_t queued() const { return count_; }

 private:
  void raise(AlertKind kind, int32_t value, int32_t extra, uint32_t now_ms);
  void expire(uint32_t now_ms);

  static constexpr uint8_t kQueue = 6;
  Alert queue_[kQueue];
  uint8_t count_ = 0;

  bool seen_ = false;
  State prev_;
  bool low_fuel_warned_ = false;
  bool short_warned_ = false;
  bool rain_soon_warned_ = false;
  bool wet_tyres_warned_ = false;
};

uint32_t alertDurationMs(AlertKind kind);

}  // namespace rigdash
