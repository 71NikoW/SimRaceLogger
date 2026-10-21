#include "check.h"
#include "rigdash/buttonbox.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

TEST(debounce_ignores_bounce) {
  Debouncer d;
  CHECK(!d.update(true, 0));
  CHECK(!d.update(false, 1));  // bounce
  CHECK(!d.update(true, 2));
  CHECK(!d.update(true, 2 + kButtonDebounceMs - 1));
  CHECK(d.update(true, 2 + kButtonDebounceMs));
  CHECK(d.update(false, 20));
  CHECK(!d.update(false, 20 + kButtonDebounceMs));
}

// One clockwise detent: 00 → 01 → 11 → 10 → 00 (a, b).
static void detent(Encoder& e, uint32_t& t) {
  static const bool seq[4][2] = {{false, true}, {true, true}, {true, false}, {false, false}};
  for (const auto& s : seq) e.update(s[0], s[1], t++);
}

TEST(encoder_steps_become_pulses) {
  Encoder e;
  uint32_t t = 0;
  e.update(false, false, t++);
  detent(e, t);
  CHECK(e.cwPressed(t));
  CHECK(!e.ccwPressed(t));
  e.update(false, false, t + kPulseMs);
  CHECK(!e.cwPressed(t + kPulseMs));  // released after the pulse
}

TEST(encoder_counter_clockwise) {
  Encoder e;
  uint32_t t = 0;
  e.update(false, false, t++);
  // CCW: 00 → 10 → 11 → 01 → 00
  e.update(true, false, t++);
  e.update(true, true, t++);
  e.update(false, true, t++);
  e.update(false, false, t++);
  CHECK(e.ccwPressed(t));
  CHECK(!e.cwPressed(t));
}

TEST(encoder_fast_spin_is_queued) {
  Encoder e;
  uint32_t t = 0;
  e.update(false, false, t++);
  for (int i = 0; i < 3; ++i) detent(e, t);
  CHECK(e.cwPressed(t));
  CHECK_EQ(e.pending(), 2);
  int presses = 1;
  bool was = true;
  for (uint32_t now = t; now < t + 1000; now += 5) {
    e.update(false, false, now);
    const bool p = e.cwPressed(now);
    if (p && !was) ++presses;
    was = p;
  }
  CHECK_EQ(presses, 3);
  CHECK_EQ(e.pending(), 0);
}

TEST(encoder_noise_without_full_detent_does_nothing) {
  Encoder e;
  uint32_t t = 0;
  e.update(false, false, t++);
  e.update(false, true, t++);
  e.update(false, false, t++);  // wobble back
  CHECK(!e.cwPressed(t) && !e.ccwPressed(t));
  CHECK_EQ(e.pending(), 0);
}

TEST(toggle_flip_is_short_press) {
  Toggle ign;
  ign.update(true, 0);            // already on at power-up: no press
  CHECK(!ign.pressed(0));
  ign.update(false, 100);
  CHECK(ign.pressed(100));
  CHECK(!ign.pressed(100 + kPulseMs));
  Toggle held(false);
  held.update(true, 0);
  CHECK(held.pressed(5000));
}

TEST(button_leds) {
  State s;
  CHECK(buttonLed(LedRole::None, true, s, true, 0).off());
  CHECK(buttonLed(LedRole::Generic, true, s, true, 0).r > buttonLed(LedRole::Generic, false, s, true, 0).r);
  s.telemetry.flags = kFlagPitLimiter | kFlagEngineOn | kFlagIgnitionOn;
  CHECK(buttonLed(LedRole::PitLimiter, false, s, true, 0) == rgb::kGreen);
  CHECK(buttonLed(LedRole::PitLimiter, false, s, true, kBlinkPeriodMs / 2).off());
  CHECK(buttonLed(LedRole::Ignition, false, s, true, 0) == rgb::kGreen);
  s.telemetry.flags = kFlagIgnitionOn;
  CHECK(buttonLed(LedRole::Ignition, false, s, true, 0) == rgb::kRed);
  CHECK(buttonLed(LedRole::Starter, false, s, true, 0) == rgb::kRed);
  CHECK(buttonLed(LedRole::Starter, false, s, true, kBlinkPeriodMs / 2).off());
  s.telemetry.race_flag = RaceFlag::Blue;
  CHECK(buttonLed(LedRole::Flag, false, s, true, 0) == rgb::kBlue);
  // Without telemetry every LED is a plain backlight.
  CHECK(!buttonLed(LedRole::Starter, false, s, false, 0).off());
  CHECK(!(buttonLed(LedRole::Starter, false, s, false, 0) == rgb::kRed));
}
