#include "check.h"
#include "rigdash/dash.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

static Engine eng(uint16_t rpm) {
  Engine e;
  e.rpm = rpm;
  e.shift_rpm = 7000;
  e.max_rpm = 7500;
  return e;
}

TEST(shift_leds_progression) {
  CHECK_EQ(shiftLedsLit(eng(5000), 8), 0);
  CHECK_EQ(shiftLedsLit(eng(6300), 8), 0);  // 90 % of shift point
  CHECK_EQ(shiftLedsLit(eng(6301), 8), 1);
  CHECK_EQ(shiftLedsLit(eng(6650), 8), 5);
  CHECK_EQ(shiftLedsLit(eng(6999), 8), 8);
  CHECK_EQ(shiftLedsLit(eng(7000), 8), 8);
  CHECK_EQ(shiftLedsLit(eng(9000), 8), 8);
  CHECK(!shiftNow(eng(6999)));
  CHECK(shiftNow(eng(7000)));
}

TEST(shift_leds_unknown_shift_point) {
  Engine e;
  e.rpm = 8000;
  CHECK_EQ(shiftLedsLit(e, 8), 0);
  CHECK(!shiftNow(e));
  CHECK_EQ(shiftLedsLit(eng(6900), 0), 0);
}

TEST(delta_bar) {
  CHECK_EQ(deltaBarPx(0, 100), 0);
  CHECK_EQ(deltaBarPx(1000, 100), 50);
  CHECK_EQ(deltaBarPx(-500, 100), -25);
  CHECK_EQ(deltaBarPx(5000, 100), 100);
  CHECK_EQ(deltaBarPx(-2147483647 - 1, 100), -100);
  CHECK_EQ(deltaBarPx(500, 100, 0), 0);
}

TEST(tyre_and_brake_bands) {
  CHECK(tyreBand(700, 800, 950) == TempBand::Cold);
  CHECK(tyreBand(850, 800, 950) == TempBand::Optimal);
  CHECK(tyreBand(800, 800, 950) == TempBand::Optimal);
  CHECK(tyreBand(990, 800, 950) == TempBand::Hot);
  CHECK(tyreBand(850, 0, 0) == TempBand::Unknown);
  CHECK(brakeBand(100) == TempBand::Cold);
  CHECK(brakeBand(450) == TempBand::Optimal);
  CHECK(brakeBand(900) == TempBand::Hot);
}

static State driving() {
  State s;
  s.telemetry.flags = kFlagEngineOn;
  return s;
}

TEST(leds_idle_blue_and_off_without_signal) {
  State s = driving();
  Leds l = ledsFor(Screen::CurrentLap, s, 0);
  CHECK(l.left == Led::Blue && l.right == Led::Blue);
  l = ledsFor(Screen::NoSignal, s, 0);
  CHECK(l.left == Led::Off && l.right == Led::Off);
  l = ledsFor(Screen::EngineOff, s, 0);
  CHECK(l.left == Led::Off);
}

TEST(leds_pit_limiter_blinks_green) {
  State s = driving();
  s.telemetry.flags |= kFlagPitLimiter;
  s.telemetry.race_flag = RaceFlag::Yellow;  // limiter wins
  CHECK(ledsFor(Screen::PitLimiter, s, 0).left == Led::Green);
  CHECK(ledsFor(Screen::PitLimiter, s, kBlinkPeriodMs / 2).left == Led::Off);
}

TEST(leds_flag_colours) {
  State s = driving();
  s.telemetry.race_flag = RaceFlag::SafetyCar;
  CHECK(ledsFor(Screen::CurrentLap, s, 0).left == Led::Yellow);
  s.telemetry.race_flag = RaceFlag::Blue;
  CHECK(ledsFor(Screen::CurrentLap, s, 0).right == Led::Blue);
  s.telemetry.race_flag = RaceFlag::Green;  // green = racing, no LED change
  CHECK(ledsFor(Screen::CurrentLap, s, 0).left == Led::Blue);
}

TEST(leds_shift_lights) {
  State s = driving();
  s.has_engine = true;
  s.engine = eng(6400);
  Leds l = ledsFor(Screen::CurrentLap, s, 0);
  CHECK(l.left == Led::Green && l.right == Led::Off);
  s.engine = eng(6800);
  l = ledsFor(Screen::CurrentLap, s, 0);
  CHECK(l.left == Led::Green && l.right == Led::Yellow);
  s.engine = eng(7100);
  CHECK(ledsFor(Screen::CurrentLap, s, 0).left == Led::Red);
  CHECK(ledsFor(Screen::CurrentLap, s, 60).left == Led::Off);
}
