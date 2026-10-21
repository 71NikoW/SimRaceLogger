#include "check.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

static Telemetry driving() {
  Telemetry t;
  t.flags = kFlagEngineOn | kFlagIgnitionOn;
  t.lap_number = 1;
  return t;
}

TEST(no_signal_before_first_frame) {
  ScreenManager m;
  CHECK(m.current(0) == Screen::NoSignal);
  CHECK(m.current(100000) == Screen::NoSignal);
}

TEST(default_page_is_current_lap) {
  ScreenManager m;
  m.onTelemetry(driving(), 1000);
  CHECK(m.current(1000) == Screen::CurrentLap);
}

TEST(signal_timeout) {
  ScreenManager m;
  m.onTelemetry(driving(), 1000);
  CHECK(m.current(1000 + kSignalTimeoutMs) == Screen::CurrentLap);
  CHECK(m.current(1001 + kSignalTimeoutMs) == Screen::NoSignal);
}

TEST(signal_survives_millis_wraparound) {
  ScreenManager m;
  m.onTelemetry(driving(), 0xFFFFFF00u);
  CHECK(m.current(0x00000100u) == Screen::CurrentLap);
}

TEST(page_cycling) {
  ScreenManager m;
  m.onTelemetry(driving(), 0);
  m.nextPage();
  CHECK(m.current(0) == Screen::TrackData);
  m.nextPage();
  CHECK(m.current(0) == Screen::LapSummary);
  m.nextPage();
  CHECK(m.current(0) == Screen::CurrentLap);
}

TEST(pit_limiter_overrides_page_and_returns) {
  ScreenManager m;
  m.nextPage();
  Telemetry t = driving();
  t.flags |= kFlagPitLimiter;
  m.onTelemetry(t, 0);
  CHECK(m.current(0) == Screen::PitLimiter);
  m.onTelemetry(driving(), 100);
  CHECK(m.current(100) == Screen::TrackData);
}

TEST(engine_off) {
  ScreenManager m;
  Telemetry t;
  t.flags = kFlagIgnitionOn;
  m.onTelemetry(t, 0);
  CHECK(m.current(0) == Screen::EngineOff);
}

TEST(lap_invalidated_overlay_is_transient) {
  ScreenManager m;
  m.onTelemetry(driving(), 0);
  Telemetry t = driving();
  t.flags |= kFlagLapInvalid;
  m.onTelemetry(t, 1000);
  CHECK(m.current(1000) == Screen::LapInvalidated);
  m.onTelemetry(t, 1000 + kInvalidOverlayMs - 1);
  CHECK(m.current(1000 + kInvalidOverlayMs - 1) == Screen::LapInvalidated);
  m.onTelemetry(t, 1000 + kInvalidOverlayMs);
  CHECK(m.current(1000 + kInvalidOverlayMs) == Screen::CurrentLap);
}

TEST(lap_invalidated_beats_pit_limiter) {
  ScreenManager m;
  Telemetry t = driving();
  t.flags |= kFlagLapInvalid | kFlagPitLimiter;
  m.onTelemetry(t, 0);
  CHECK(m.current(0) == Screen::LapInvalidated);
  m.onTelemetry(t, kInvalidOverlayMs);
  CHECK(m.current(kInvalidOverlayMs) == Screen::PitLimiter);
}

TEST(lap_invalidated_retriggers_on_new_lap) {
  ScreenManager m;
  Telemetry t = driving();
  t.flags |= kFlagLapInvalid;
  m.onTelemetry(t, 0);
  m.onTelemetry(t, 10000);
  CHECK(m.current(10000) == Screen::CurrentLap);
  t.lap_number = 2;
  m.onTelemetry(t, 10100);
  CHECK(m.current(10100) == Screen::LapInvalidated);
}

TEST(no_signal_beats_everything) {
  ScreenManager m;
  Telemetry t = driving();
  t.flags |= kFlagLapInvalid | kFlagPitLimiter;
  m.onTelemetry(t, 0);
  CHECK(m.current(kSignalTimeoutMs + 1) == Screen::NoSignal);
}

TEST(blink_phase) {
  CHECK(blinkOn(0));
  CHECK(blinkOn(kBlinkPeriodMs / 2 - 1));
  CHECK(!blinkOn(kBlinkPeriodMs / 2));
  CHECK(!blinkOn(kBlinkPeriodMs - 1));
  CHECK(blinkOn(kBlinkPeriodMs));
}

TEST(flag_change_shows_overlay_then_returns) {
  ScreenManager m;
  Telemetry t = driving();
  t.race_flag = RaceFlag::Green;
  m.onTelemetry(t, 0);
  CHECK(m.current(0) == Screen::CurrentLap);  // first frame: no overlay
  t.race_flag = RaceFlag::Yellow;
  m.onTelemetry(t, 1000);
  CHECK(m.current(1000) == Screen::RaceFlag);
  CHECK(m.raceFlag() == RaceFlag::Yellow);
  m.onTelemetry(t, 1000 + kFlagOverlayMs);
  CHECK(m.current(1000 + kFlagOverlayMs) == Screen::CurrentLap);
  CHECK(m.raceFlag() == RaceFlag::Yellow);  // strip stays while waved
}

TEST(flag_withdrawn_shows_no_overlay) {
  ScreenManager m;
  Telemetry t = driving();
  t.race_flag = RaceFlag::Blue;
  m.onTelemetry(t, 0);
  t.race_flag = RaceFlag::None;
  m.onTelemetry(t, 5000);
  CHECK(m.current(5000) == Screen::CurrentLap);
}

TEST(flag_overlay_beats_pit_limiter_and_invalid_lap) {
  ScreenManager m;
  m.onTelemetry(driving(), 0);
  Telemetry t = driving();
  t.flags |= kFlagPitLimiter | kFlagLapInvalid;
  t.race_flag = RaceFlag::Black;
  m.onTelemetry(t, 100);
  CHECK(m.current(100) == Screen::RaceFlag);
  m.onTelemetry(t, 100 + kFlagOverlayMs);
  CHECK(m.current(100 + kFlagOverlayMs) == Screen::PitLimiter);
}
