#include "rigdash/screen_manager.h"

namespace rigdash {

void ScreenManager::onTelemetry(const Telemetry& t, uint32_t now_ms) {
  const bool was_invalid = have_data_ && last_.has(kFlagLapInvalid);
  // A new lap with the flag still set (some sims) counts as a fresh invalidation.
  const bool new_lap = have_data_ && t.lap_number != last_.lap_number;
  if (t.has(kFlagLapInvalid) && (!was_invalid || new_lap)) {
    invalid_overlay_ = true;
    invalid_since_ms_ = now_ms;
  }
  // Show the flag full-screen when it changes; not for the very first frame
  // (connecting mid-session) and not when the flag is withdrawn.
  if (have_data_ && t.race_flag != last_.race_flag && t.race_flag != RaceFlag::None) {
    flag_overlay_ = true;
    flag_since_ms_ = now_ms;
  }
  last_ = t;
  have_data_ = true;
  last_rx_ms_ = now_ms;
}

void ScreenManager::nextPage() {
  switch (page_) {
    case Screen::CurrentLap: page_ = Screen::TrackData; break;
    case Screen::TrackData: page_ = Screen::LapSummary; break;
    default: page_ = Screen::CurrentLap; break;
  }
}

bool ScreenManager::hasSignal(uint32_t now_ms) const {
  // Unsigned subtraction keeps working across the millis() wrap-around.
  return have_data_ && (now_ms - last_rx_ms_) <= kSignalTimeoutMs;
}

Screen ScreenManager::current(uint32_t now_ms) const {
  if (!hasSignal(now_ms)) return Screen::NoSignal;
  if (flag_overlay_ && (now_ms - flag_since_ms_) < kFlagOverlayMs) return Screen::RaceFlag;
  if (invalid_overlay_ && (now_ms - invalid_since_ms_) < kInvalidOverlayMs) {
    return Screen::LapInvalidated;
  }
  if (last_.has(kFlagPitLimiter)) return Screen::PitLimiter;
  if (!last_.has(kFlagEngineOn)) return Screen::EngineOff;
  return page_;
}

}  // namespace rigdash
