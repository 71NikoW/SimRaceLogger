#include "check.h"
#include "rigdash/alerts.h"

using namespace rigdash;

static State base() {
  State s;
  s.telemetry.flags = kFlagEngineOn;
  s.telemetry.fuel_laps_x10 = 100;
  s.has_weather = s.has_car = s.has_race = s.has_fuel = s.has_tyres = true;
  s.car.abs = 3;
  s.car.tc = 4;
  s.car.tc_cut = 2;
  s.car.engine_map = 1;
  s.car.brake_bias_x10 = 560;
  s.fuel.to_finish_x10 = -50;
  s.tyres.compound = Compound::Medium;
  return s;
}

// Connect with `s`, then apply each change and return the visible alert.
static Alert after(AlertEngine& e, const State& s, uint32_t now) {
  e.update(s, now);
  return e.current(now);
}

TEST(no_alert_for_initial_state) {
  AlertEngine e;
  State s = base();
  s.weather.rain_in_10 = 3;      // rain already forecast at connect
  s.race.penalty = Penalty::DriveThrough;
  s.telemetry.fuel_laps_x10 = 10;
  CHECK(after(e, s, 0).kind == AlertKind::None);
  CHECK(after(e, s, 100).kind == AlertKind::None);
}

TEST(rain_soon_fires_once) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.weather.rain_in_10 = 2;
  Alert a = after(e, s, 100);
  CHECK(a.kind == AlertKind::RainSoon);
  CHECK_EQ(a.value, 2);
  e.update(s, 5000);
  CHECK(e.current(5000).kind == AlertKind::None);
  s.weather.rain_in_10 = 3;  // forecast worsens, still the same warning
  CHECK(after(e, s, 6000).kind == AlertKind::None);
}

TEST(rain_started_and_increasing_and_drying) {
  AlertEngine e;
  State s = base();
  s.tyres.compound = Compound::Wet;
  e.update(s, 0);
  s.weather.rain = 2;
  s.weather.rain_in_10 = 2;
  CHECK(after(e, s, 100).kind == AlertKind::RainStarted);
  s.weather.rain = 4;
  e.update(s, 4000);
  CHECK(e.current(4000).kind == AlertKind::RainIncreasing);
  s.weather.rain = 0;
  s.weather.rain_in_10 = 0;
  s.weather.wetness = 4;
  e.update(s, 8000);
  s.weather.wetness = 3;
  e.update(s, 12000);
  CHECK(e.current(12000).kind == AlertKind::TrackDrying);
}

TEST(wet_tyres_advised_on_slicks) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.weather.rain = 3;
  e.update(s, 100);
  CHECK(e.current(100).kind == AlertKind::RainStarted);
  CHECK(e.queued() == 2);
  e.update(s, 100 + kAlertMs);
  CHECK(e.current(100 + kAlertMs).kind == AlertKind::WetTyresAdvised);
}

TEST(low_fuel_with_hysteresis) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.telemetry.fuel_laps_x10 = 19;
  CHECK(after(e, s, 100).kind == AlertKind::LowFuel);
  s.telemetry.fuel_laps_x10 = 21;  // noise around the threshold
  e.update(s, 5000);
  s.telemetry.fuel_laps_x10 = 18;
  CHECK(after(e, s, 5100).kind == AlertKind::None);
  s.telemetry.fuel_laps_x10 = 150;  // refuelled
  e.update(s, 9000);
  s.telemetry.fuel_laps_x10 = 15;
  CHECK(after(e, s, 9100).kind == AlertKind::LowFuel);
}

TEST(fuel_short_to_finish) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.fuel.to_finish_x10 = 32;
  Alert a = after(e, s, 100);
  CHECK(a.kind == AlertKind::FuelShort);
  CHECK_EQ(a.value, 32);
}

TEST(setting_change_popups_coalesce) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.car.brake_bias_x10 = 555;
  e.update(s, 100);
  s.car.brake_bias_x10 = 550;
  e.update(s, 300);
  Alert a = e.current(300);
  CHECK(a.kind == AlertKind::BrakeBias);
  CHECK_EQ(a.value, 550);
  CHECK_EQ(e.queued(), 1);
  // Timer restarted at 300: still visible shortly before 300 + 1500.
  CHECK(e.current(300 + kSettingAlertMs - 1).kind == AlertKind::BrakeBias);
  e.update(s, 300 + kSettingAlertMs);
  CHECK(e.current(300 + kSettingAlertMs).kind == AlertKind::None);
}

TEST(abs_tc_map_changes) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.car.abs = 4;
  s.car.tc = 5;
  s.car.engine_map = 2;
  e.update(s, 10);
  CHECK(e.current(10).kind == AlertKind::Abs);
  CHECK_EQ(e.queued(), 3);
  e.update(s, 10 + kSettingAlertMs);
  CHECK(e.current(10 + kSettingAlertMs).kind == AlertKind::Tc);
}

TEST(setting_not_available_is_ignored) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.car.abs = kSettingNa;
  CHECK(after(e, s, 10).kind == AlertKind::None);
}

TEST(damage_penalty_track_limits_last_lap_pit_window) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  s.car.damage_pct = 3;  // below step
  CHECK(after(e, s, 10).kind == AlertKind::None);
  s.car.damage_pct = 12;
  CHECK(after(e, s, 20).kind == AlertKind::Damage);

  AlertEngine r;
  State t = base();
  r.update(t, 0);
  t.race.penalty = Penalty::TimePenalty;
  t.race.penalty_s = 5;
  t.race.track_limits = 3;
  t.race.track_limits_max = 4;
  t.race.bits = kRaceLastLap;
  t.race.pit_window = PitWindow::Open;
  r.update(t, 10);
  Alert a = r.current(10);
  CHECK(a.kind == AlertKind::Penalty);
  CHECK_EQ(a.extra, 5);
  CHECK_EQ(r.queued(), 4);
  r.update(t, 10 + kAlertMs);
  a = r.current(10 + kAlertMs);
  CHECK(a.kind == AlertKind::TrackLimits);
  CHECK_EQ(a.value, 3);
  CHECK_EQ(a.extra, 4);
}

TEST(queue_is_bounded) {
  AlertEngine e;
  State s = base();
  e.update(s, 0);
  for (int i = 1; i <= 20; ++i) {
    s.race.track_limits = static_cast<uint8_t>(i);
    s.car.damage_pct = static_cast<uint8_t>(i * 5);
    s.car.abs = static_cast<uint8_t>(i);
    e.update(s, static_cast<uint32_t>(i));
  }
  CHECK(e.queued() <= 6);
}

TEST(first_weather_frame_is_baseline) {
  AlertEngine e;
  State s = base();
  s.has_weather = false;
  e.update(s, 0);
  s.has_weather = true;
  s.weather.rain = 4;
  s.weather.rain_in_10 = 4;
  CHECK(after(e, s, 10).kind == AlertKind::None);
}
