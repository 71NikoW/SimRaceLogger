#include <vector>

#include "check.h"
#include "rigdash/flag_panel.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

static Telemetry withFlag(RaceFlag f) {
  Telemetry t;
  t.flags = kFlagEngineOn;
  t.race_flag = f;
  return t;
}

static std::vector<Rgb> frame(const FlagPanel& p, uint32_t now) {
  std::vector<Rgb> out(p.width() * p.height());
  p.render(now, out.data());
  return out;
}

static int count(const std::vector<Rgb>& f, Rgb c) {
  int n = 0;
  for (const Rgb& p : f) n += p == c;
  return n;
}

TEST(serpentine_index) {
  CHECK_EQ(serpentineIndex(0, 0, 16), 0);
  CHECK_EQ(serpentineIndex(15, 0, 16), 15);
  CHECK_EQ(serpentineIndex(0, 1, 16), 31);   // second row runs backwards
  CHECK_EQ(serpentineIndex(15, 1, 16), 16);
  CHECK_EQ(serpentineIndex(3, 2, 16), 35);
}

TEST(panel_no_signal_scanner) {
  FlagPanel p;
  CHECK(p.mode(0) == PanelMode::NoSignal);
  CHECK_EQ(256 - count(frame(p, 1000), rgb::kOff), 1);
}

TEST(panel_dark_while_racing_green_flashes_briefly) {
  FlagPanel p;
  p.onTelemetry(withFlag(RaceFlag::Green), 0);
  CHECK(p.mode(0) == PanelMode::Flag);
  CHECK_EQ(count(frame(p, 0), rgb::kGreen), 256);
  p.onTelemetry(withFlag(RaceFlag::Green), kGreenFlashMs);
  CHECK(p.mode(kGreenFlashMs) == PanelMode::Off);
  CHECK_EQ(count(frame(p, kGreenFlashMs), rgb::kOff), 256);
}

TEST(panel_yellow_blinks) {
  FlagPanel p;
  p.onTelemetry(withFlag(RaceFlag::Yellow), 0);
  CHECK_EQ(count(frame(p, 0), rgb::kYellow), 256);
  CHECK_EQ(count(frame(p, kBlinkPeriodMs / 2), rgb::kOff), 256);
}

TEST(panel_blue_has_yellow_stripe) {
  FlagPanel p;
  p.onTelemetry(withFlag(RaceFlag::Blue), 0);
  auto f = frame(p, 0);
  CHECK(count(f, rgb::kBlue) > 180);
  CHECK(count(f, rgb::kYellow) >= 32);
  CHECK(f[15 * 16 + 0] == rgb::kYellow);   // bottom-left
  CHECK(f[0 * 16 + 15] == rgb::kYellow);   // top-right
  CHECK(f[0] == rgb::kBlue);               // top-left stays blue
}

TEST(panel_chequered_alternates) {
  FlagPanel p;
  p.onTelemetry(withFlag(RaceFlag::Chequered), 0);
  auto a = frame(p, 0), b = frame(p, kBlinkPeriodMs / 2);
  CHECK_EQ(count(a, rgb::kWhite), 128);
  CHECK(!(a[0] == b[0]));
  CHECK(a[0] == a[3 * 16 + 3]);       // same 4×4 square
  CHECK(!(a[0] == a[4]));             // next square differs
}

TEST(panel_safety_car_text) {
  FlagPanel p;
  p.onTelemetry(withFlag(RaceFlag::SafetyCar), 0);
  auto on = frame(p, 0);
  int text = count(on, rgb::kOff);
  CHECK(text > 20 && text < 100);     // black "SC" on yellow
  auto off = frame(p, kBlinkPeriodMs / 2);
  CHECK_EQ(count(off, rgb::kYellow), text);  // inverted phase
  FlagPanel v;
  v.onTelemetry(withFlag(RaceFlag::VirtualSafetyCar), 0);
  int vtext = count(frame(v, 0), rgb::kOff);
  CHECK(vtext > 10 && vtext < 60);    // smaller 1× "VSC"
}

TEST(panel_black_red_white) {
  FlagPanel p;
  p.onTelemetry(withFlag(RaceFlag::Black), 0);
  auto f = frame(p, 0);
  CHECK_EQ(count(f, rgb::kWhite), 60);  // 16×16 frame
  CHECK(f[8 * 16 + 8] == rgb::kOff);
  p.onTelemetry(withFlag(RaceFlag::Red), 10);
  CHECK_EQ(count(frame(p, 10), rgb::kRed), 256);
  p.onTelemetry(withFlag(RaceFlag::White), 20);
  CHECK_EQ(count(frame(p, 20), rgb::kWhite), 256);
}

TEST(panel_pit_limiter_and_delta_idle) {
  FlagPanel p;
  Telemetry t = withFlag(RaceFlag::None);
  t.flags |= kFlagPitLimiter;
  p.onTelemetry(t, 0);
  CHECK(p.mode(0) == PanelMode::PitLimiter);
  CHECK_EQ(count(frame(p, 0), rgb::kGreen), 60);
  t.flags = kFlagEngineOn;
  t.best_lap_ms = 90000;
  t.delta_ms = -100;
  p.onTelemetry(t, 10);
  CHECK(p.mode(10) == PanelMode::Off);
  p.setDeltaIdle(true);
  CHECK_EQ(count(frame(p, 10), rgb::kDimGreen), 256);
}

TEST(panel_flag_beats_pit_limiter) {
  FlagPanel p;
  Telemetry t = withFlag(RaceFlag::Yellow);
  t.flags |= kFlagPitLimiter;
  p.onTelemetry(t, 0);
  CHECK(p.mode(0) == PanelMode::Flag);
}

TEST(panel_small_matrix) {
  FlagPanel p(8, 8);
  p.onTelemetry(withFlag(RaceFlag::SafetyCar), 0);
  std::vector<Rgb> out(64);
  p.render(0, out.data());  // must not write out of bounds (ASan)
  p.onTelemetry(withFlag(RaceFlag::Blue), 10);
  p.render(10, out.data());
  CHECK(out[7 * 8] == rgb::kYellow);
}
