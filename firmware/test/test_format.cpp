#include "check.h"
#include "rigdash/format.h"

using namespace rigdash;

TEST(lap_time_basic) {
  char b[16];
  CHECK_STR(formatLapTime(88200, b, sizeof b), "1:28.2");
  CHECK_STR(formatLapTime(133700, b, sizeof b), "2:13.7");
  CHECK_STR(formatLapTime(228500, b, sizeof b), "3:48.5");
  CHECK_STR(formatLapTime(5000, b, sizeof b), "0:05.0");
}

TEST(lap_time_truncates_not_rounds) {
  char b[16];
  CHECK_STR(formatLapTime(88299, b, sizeof b), "1:28.2");
  CHECK_STR(formatLapTime(59999, b, sizeof b), "0:59.9");
}

TEST(lap_time_decimals) {
  char b[16];
  CHECK_STR(formatLapTime(92734, b, sizeof b, 3), "1:32.734");
  CHECK_STR(formatLapTime(92734, b, sizeof b, 2), "1:32.73");
  CHECK_STR(formatLapTime(92734, b, sizeof b, 9), "1:32.734");
}

TEST(lap_time_none_and_long) {
  char b[16];
  CHECK_STR(formatLapTime(0, b, sizeof b), "-:--.-");
  CHECK_STR(formatLapTime(0, b, sizeof b, 3), "-:--.---");
  CHECK_STR(formatLapTime(723400, b, sizeof b), "12:03.4");
}

TEST(lap_time_small_buffer_is_safe) {
  char b[4];
  formatLapTime(88200, b, sizeof b);
  CHECK_EQ(strlen(b), 3u);
}

TEST(delta) {
  char b[16];
  CHECK_STR(formatDelta(350, b, sizeof b), "+0.35");
  CHECK_STR(formatDelta(-1204, b, sizeof b), "-1.20");
  CHECK_STR(formatDelta(0, b, sizeof b), "+0.00");
  CHECK_STR(formatDelta(250000, b, sizeof b), "+99.99");
  CHECK_STR(formatDelta(-2147483647 - 1, b, sizeof b), "-99.99");
}

TEST(fuel_laps) {
  char b[16];
  CHECK_STR(formatFuelLaps(0, b, sizeof b), "0.0 LAPS");
  CHECK_STR(formatFuelLaps(123, b, sizeof b), "12.3 LAPS");
  CHECK_STR(formatFuelLaps(0xFFFF, b, sizeof b), "--.- LAPS");
}

TEST(gear) {
  char b[4];
  CHECK_STR(formatGear(-1, b, sizeof b), "R");
  CHECK_STR(formatGear(0, b, sizeof b), "N");
  CHECK_STR(formatGear(6, b, sizeof b), "6");
}

TEST(race_flag_text) {
  CHECK_STR(raceFlagText(RaceFlag::Yellow), "YELLOW FLAG");
  CHECK_STR(raceFlagText(RaceFlag::Chequered), "CHEQUERED FLAG");
  CHECK_STR(raceFlagText(RaceFlag::SafetyCar), "SAFETY CAR");
  CHECK_STR(raceFlagText(RaceFlag::VirtualSafetyCar), "VIRTUAL SAFETY CAR");
  CHECK_STR(raceFlagText(RaceFlag::None), "");
  CHECK_STR(raceFlagText(static_cast<RaceFlag>(42)), "");
}

TEST(temperature) {
  char b[8];
  CHECK_STR(formatTemp(261, b, sizeof b), "26.1");
  CHECK_STR(formatTemp(-55, b, sizeof b), "-5.5");
  CHECK_STR(formatTemp(-5, b, sizeof b), "-0.5");
  CHECK_STR(formatTemp(0, b, sizeof b), "0.0");
}

TEST(units) {
  char b[16];
  CHECK_STR(formatTemp(261, b, sizeof b, true), "79.0");
  CHECK_STR(formatTemp(0, b, sizeof b, true), "32.0");
  CHECK_STR(formatTemp(-400, b, sizeof b, true), "-40.0");
  CHECK_STR(formatSpeed(187, b, sizeof b), "187");
  CHECK_STR(formatSpeed(187, b, sizeof b, true), "116");
  CHECK_STR(formatSpeed(100, b, sizeof b, true), "62");
  CHECK_STR(formatPressure(2750, b, sizeof b), "27.5");
  CHECK_STR(formatPressure(2747, b, sizeof b), "27.5");
  CHECK_STR(formatPressure(2750, b, sizeof b, true), "1.90");
  CHECK_STR(formatPressure(65535, b, sizeof b, true), "45.18");
}

TEST(gap_duration_liters_bias_setting) {
  char b[16];
  CHECK_STR(formatGap(1234, b, sizeof b), "+1.2");
  CHECK_STR(formatGap(-850, b, sizeof b), "-0.8");
  CHECK_STR(formatGap(kGapUnknown, b, sizeof b), "--.-");
  CHECK_STR(formatGap(5000000, b, sizeof b), "+999.9");
  CHECK_STR(formatDuration(2712, b, sizeof b), "45:12");
  CHECK_STR(formatDuration(3753, b, sizeof b), "1:02:33");
  CHECK_STR(formatDuration(0, b, sizeof b), "0:00");
  CHECK_STR(formatDuration(kTimeLeftUnknown, b, sizeof b), "--:--");
  CHECK_STR(formatLiters(425, b, sizeof b), "42.5 L");
  CHECK_STR(formatBias(565, b, sizeof b), "56.5%");
  CHECK_STR(formatBias(0, b, sizeof b), "--.-%");
  CHECK_STR(formatSetting(3, b, sizeof b), "3");
  CHECK_STR(formatSetting(kSettingNa, b, sizeof b), "-");
}

TEST(texts) {
  CHECK_STR(rainText(0), "DRY");
  CHECK_STR(rainText(4), "HEAVY RAIN");
  CHECK_STR(rainText(200), "STORM");
  CHECK_STR(penaltyText(Penalty::StopGo), "STOP & GO");
  CHECK_STR(penaltyText(Penalty::None), "");
  CHECK_STR(compoundText(Compound::Inter), "INTER");
  CHECK_STR(compoundText(Compound::Unknown), "");
}
