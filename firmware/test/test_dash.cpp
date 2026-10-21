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

#include <vector>

#include "rigdash/alerts.h"

// Returns the strip in logical order — top 0..11, left 12..15, right 16..19
// (each side top→bottom) — so tests read naturally; chain order is checked
// separately below.
static std::vector<Rgb> strip(Screen sc, const State& s, uint32_t now, AlertKind a = AlertKind::None,
                              StripLayout l = StripLayout{}) {
  std::vector<Rgb> chain(l.count()), out;
  stripFor(sc, s, a, now, l, chain.data());
  for (uint8_t i = 0; i < l.top; ++i) out.push_back(chain[l.topAt(i)]);
  for (uint8_t i = 0; i < l.side; ++i) out.push_back(chain[l.left(i)]);
  for (uint8_t i = 0; i < l.side; ++i) out.push_back(chain[l.right(i)]);
  return out;
}

TEST(strip_chain_order_matches_wiring) {
  StripLayout l;  // 12 top, 4 per side
  CHECK_EQ(l.left(3), 0);    // chain starts bottom-left
  CHECK_EQ(l.left(0), 3);    // top of the left bar
  CHECK_EQ(l.topAt(0), 4);   // then the top bar, left to right
  CHECK_EQ(l.topAt(11), 15);
  CHECK_EQ(l.right(0), 16);  // then the right bar, top to bottom
  CHECK_EQ(l.right(3), 19);
  State s = driving();
  s.has_engine = true;
  s.engine = eng(6310);  // only the leftmost top LED lit
  std::vector<Rgb> chain(l.count());
  stripFor(Screen::CurrentLap, s, AlertKind::None, 0, l, chain.data());
  CHECK(chain[4] == rgb::kGreen);
  CHECK(chain[5].off());
}

TEST(strip_shift_lights_fill_and_flash) {
  State s = driving();
  s.has_engine = true;
  s.engine = eng(6000);
  auto o = strip(Screen::CurrentLap, s, 0);
  for (int i = 0; i < 12; ++i) CHECK(o[i].off());
  s.engine = eng(6999);
  o = strip(Screen::CurrentLap, s, 0);
  CHECK(o[0] == rgb::kGreen && o[5] == rgb::kGreen);
  CHECK(o[6] == rgb::kYellow && o[8] == rgb::kYellow);
  CHECK(o[9] == rgb::kRed && o[11] == rgb::kRed);
  s.engine = eng(6650);
  o = strip(Screen::CurrentLap, s, 0);
  CHECK(!o[5].off() && o[7].off());
  s.engine = eng(7200);
  CHECK(strip(Screen::CurrentLap, s, 0)[3] == rgb::kBlue);
  CHECK(strip(Screen::CurrentLap, s, 60)[3].off());
}

TEST(strip_pit_limiter_alternates) {
  State s = driving();
  s.telemetry.flags |= kFlagPitLimiter;
  auto a = strip(Screen::PitLimiter, s, 0);
  auto b = strip(Screen::PitLimiter, s, kBlinkPeriodMs / 2);
  CHECK(a[0] == rgb::kBlue && a[11].off());
  CHECK(b[0].off() && b[11] == rgb::kBlue);
  CHECK(a[12] == rgb::kGreen && b[12].off());
}

TEST(strip_flags_on_sides) {
  State s = driving();
  s.telemetry.race_flag = RaceFlag::Yellow;
  auto o = strip(Screen::CurrentLap, s, 0);
  for (int i = 12; i < 20; ++i) CHECK(o[i] == rgb::kYellow);
  CHECK(strip(Screen::CurrentLap, s, kBlinkPeriodMs / 2)[12].off());
  s.telemetry.race_flag = RaceFlag::SafetyCar;  // left/right take turns
  o = strip(Screen::CurrentLap, s, 0);
  CHECK(o[12] == rgb::kYellow && o[16].off());
  CHECK(o[13].off() && o[17] == rgb::kYellow);
}

TEST(strip_alert_invalid_delta_and_idle) {
  State s = driving();
  CHECK(strip(Screen::CurrentLap, s, 0)[12] == rgb::kDimBlue);
  s.telemetry.best_lap_ms = 90000;
  s.telemetry.delta_ms = -200;
  CHECK(strip(Screen::CurrentLap, s, 0)[15] == rgb::kDimGreen);
  s.telemetry.delta_ms = 300;
  CHECK(strip(Screen::CurrentLap, s, 0)[19] == rgb::kDimRed);
  CHECK(strip(Screen::Alert, s, 0, AlertKind::RainSoon)[12] == rgb::kCyan);
  CHECK(strip(Screen::Alert, s, 0, AlertKind::LowFuel)[12] == rgb::kOrange);
  CHECK(strip(Screen::LapInvalidated, s, 0)[12] == rgb::kRed);
}

TEST(strip_no_signal_scanner_and_engine_off) {
  State s = driving();
  int lit = 0;
  for (const Rgb& c : strip(Screen::NoSignal, s, 1234)) lit += !c.off();
  CHECK_EQ(lit, 1);
  for (const Rgb& c : strip(Screen::EngineOff, s, 0)) CHECK(c.off());
}

TEST(strip_custom_layouts) {
  State s = driving();
  s.has_engine = true;
  s.engine = eng(6999);
  StripLayout l;
  l.top = 16;
  l.side = 0;
  auto o = strip(Screen::CurrentLap, s, 0, AlertKind::None, l);
  CHECK_EQ(o.size(), 16u);
  CHECK(o[15] == rgb::kRed);
  l.top = 0;
  l.side = 3;
  o = strip(Screen::CurrentLap, s, 0, AlertKind::None, l);
  CHECK_EQ(o.size(), 6u);
  l.top = 1;  // single LED: scanner must stay in range
  l.side = 0;
  for (uint32_t t = 0; t < 5000; t += 37) {
    o = strip(Screen::NoSignal, s, t, AlertKind::None, l);
    CHECK_EQ(o.size(), 1u);
  }
}

TEST(brightness_limiter) {
  std::vector<Rgb> px(256, rgb::kWhite);
  // 256 × 60 mA = 15 A at full: must be throttled hard under 450 mA.
  const uint8_t b = limitBrightness(px.data(), 256, 255, 450);
  CHECK(b > 0 && b < 10);
  const uint32_t ma = 256 + 256u * 3u * 20u * b / 255u;
  CHECK(ma <= 450);
  // A few LEDs fit easily.
  std::vector<Rgb> few(20, rgb::kOff);
  few[0] = rgb::kRed;
  CHECK_EQ(limitBrightness(few.data(), 20, 200, 450), 200);
  CHECK_EQ(limitBrightness(few.data(), 20, 200, 10), 0);  // budget below idle current
  std::vector<Rgb> dark(256, rgb::kOff);
  CHECK_EQ(limitBrightness(dark.data(), 256, 255, 450), 255);
}
