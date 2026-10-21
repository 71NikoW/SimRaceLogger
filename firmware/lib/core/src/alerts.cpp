#include "rigdash/alerts.h"

namespace rigdash {

namespace {
bool isSetting(AlertKind k) {
  return k == AlertKind::Abs || k == AlertKind::Tc || k == AlertKind::TcCut ||
         k == AlertKind::EngineMap || k == AlertKind::BrakeBias;
}
bool dryCompound(Compound c) {
  return c == Compound::Soft || c == Compound::Medium || c == Compound::Hard;
}
constexpr uint8_t kRainLight = 2;
constexpr uint8_t kRainMedium = 3;
}  // namespace

uint32_t alertDurationMs(AlertKind kind) { return isSetting(kind) ? kSettingAlertMs : kAlertMs; }

void AlertEngine::raise(AlertKind kind, int32_t value, int32_t extra, uint32_t now) {
  // Same kind already queued (e.g. brake bias clicked 5 times): update it in
  // place and restart its timer instead of queueing five popups.
  for (uint8_t i = 0; i < count_; ++i) {
    if (queue_[i].kind == kind) {
      queue_[i].value = value;
      queue_[i].extra = extra;
      if (i == 0) queue_[i].since_ms = now;
      return;
    }
  }
  if (count_ == kQueue) {
    // Drop the oldest waiting (not the visible) alert.
    for (uint8_t i = 1; i + 1 < kQueue; ++i) queue_[i] = queue_[i + 1];
    --count_;
  }
  queue_[count_++] = Alert{kind, value, extra, now};
}

void AlertEngine::expire(uint32_t now) {
  while (count_ > 0 && now - queue_[0].since_ms >= alertDurationMs(queue_[0].kind)) {
    for (uint8_t i = 0; i + 1 < count_; ++i) queue_[i] = queue_[i + 1];
    --count_;
    if (count_ > 0) queue_[0].since_ms = now;  // next one starts now
  }
}

void AlertEngine::update(const State& s, uint32_t now) {
  expire(now);
  // Don't announce what was already true when the display connected
  // mid-session: the first time a message arrives only sets the baseline.
  const State& p = prev_;
  if (!seen_) low_fuel_warned_ = s.telemetry.fuel_laps_x10 < kLowFuelLapsX10;
  if (s.has_weather && (!seen_ || !p.has_weather)) {
    rain_soon_warned_ = s.weather.rain < kRainLight && s.weather.rain_in_10 >= kRainLight;
    const bool wet = s.weather.rain >= kRainMedium || s.weather.wetness >= kRainMedium;
    wet_tyres_warned_ = wet && s.has_tyres && dryCompound(s.tyres.compound);
  }
  if (s.has_fuel && (!seen_ || !p.has_fuel)) {
    short_warned_ = s.fuel.to_finish_x10 > 0 && s.fuel.to_finish_x10 != kFuelToFinishUnknown;
  }
  if (!seen_) {
    prev_ = s;
    seen_ = true;
    return;
  }

  // --- Weather ---
  if (s.has_weather && p.has_weather) {
    const Weather& w = s.weather;
    const Weather& pw = p.weather;
    const bool had = true;
    if (w.rain < kRainLight && w.rain_in_10 >= kRainLight) {
      if (!rain_soon_warned_) raise(AlertKind::RainSoon, w.rain_in_10, 0, now);
      rain_soon_warned_ = true;
    } else if (w.rain_in_10 < kRainLight) {
      rain_soon_warned_ = false;
    }
    if (had && w.rain >= kRainLight && pw.rain < kRainLight) {
      raise(AlertKind::RainStarted, w.rain, 0, now);
    } else if (had && w.rain > pw.rain && pw.rain >= kRainLight) {
      raise(AlertKind::RainIncreasing, w.rain, 0, now);
    }
    if (had && w.rain == 0 && w.wetness < pw.wetness) raise(AlertKind::TrackDrying, w.wetness, 0, now);
    const bool wet = w.rain >= kRainMedium || w.wetness >= kRainMedium;
    if (wet && s.has_tyres && dryCompound(s.tyres.compound)) {
      if (!wet_tyres_warned_) raise(AlertKind::WetTyresAdvised, w.rain, 0, now);
      const bool wet = s.weather.rain >= kRainMedium || s.weather.wetness >= kRainMedium;
    wet_tyres_warned_ = wet && s.has_tyres && dryCompound(s.tyres.compound);
    } else {
      wet_tyres_warned_ = false;
    }
  }

  // --- Fuel ---
  const uint16_t fl = s.telemetry.fuel_laps_x10;
  if (fl != kFuelUnknown && fl < kLowFuelLapsX10 && !s.telemetry.has(kFlagInPit)) {
    if (!low_fuel_warned_) raise(AlertKind::LowFuel, fl, 0, now);
    low_fuel_warned_ = true;
  } else if (fl == kFuelUnknown || fl >= kLowFuelLapsX10 + 5) {
    low_fuel_warned_ = false;  // refuelled (with hysteresis)
  }
  if (s.has_fuel && p.has_fuel && s.fuel.to_finish_x10 != kFuelToFinishUnknown) {
    if (s.fuel.to_finish_x10 > 0) {
      if (!short_warned_) raise(AlertKind::FuelShort, s.fuel.to_finish_x10, 0, now);
      short_warned_ = true;
    } else {
      short_warned_ = false;
    }
  }

  // --- Car ---
  if (s.has_car && p.has_car) {
    const Car& c = s.car;
    const Car& pc = p.car;
    if (c.damage_pct >= pc.damage_pct + kDamageStepPct) raise(AlertKind::Damage, c.damage_pct, 0, now);
    if (c.abs != pc.abs && c.abs != kSettingNa) raise(AlertKind::Abs, c.abs, 0, now);
    if (c.tc != pc.tc && c.tc != kSettingNa) raise(AlertKind::Tc, c.tc, 0, now);
    if (c.tc_cut != pc.tc_cut && c.tc_cut != kSettingNa) raise(AlertKind::TcCut, c.tc_cut, 0, now);
    if (c.engine_map != pc.engine_map && c.engine_map != kSettingNa) {
      raise(AlertKind::EngineMap, c.engine_map, 0, now);
    }
    if (c.brake_bias_x10 != pc.brake_bias_x10 && c.brake_bias_x10 != 0 && pc.brake_bias_x10 != 0) {
      raise(AlertKind::BrakeBias, c.brake_bias_x10, 0, now);
    }
  }

  // --- Race ---
  if (s.has_race && p.has_race) {
    const Race& r = s.race;
    const Race& pr = p.race;
    if (r.penalty != Penalty::None && r.penalty != pr.penalty) {
      raise(AlertKind::Penalty, static_cast<int32_t>(r.penalty), r.penalty_s, now);
    }
    if (r.track_limits > pr.track_limits) raise(AlertKind::TrackLimits, r.track_limits, r.track_limits_max, now);
    if ((r.bits & kRaceLastLap) && !(pr.bits & kRaceLastLap)) raise(AlertKind::LastLap, 0, 0, now);
    if (r.pit_window == PitWindow::Open && pr.pit_window != PitWindow::Open) {
      raise(AlertKind::PitWindowOpen, 0, 0, now);
    }
  }

  prev_ = s;
}

Alert AlertEngine::current(uint32_t now) const {
  if (count_ == 0) return Alert{};
  const Alert& a = queue_[0];
  if (now - a.since_ms >= alertDurationMs(a.kind)) {
    // Expired but update() not called yet; the next one is shown on update.
    return count_ > 1 ? queue_[1] : Alert{};
  }
  return a;
}

}  // namespace rigdash
