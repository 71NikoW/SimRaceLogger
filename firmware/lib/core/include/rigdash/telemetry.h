#pragma once
// Decoded display state. One struct per protocol message (docs/PROTOCOL.md).
#include <stdint.h>

namespace rigdash {

constexpr uint16_t kFuelUnknown = 0xFFFF;
constexpr int32_t kGapUnknown = 0x7FFFFFFF;
constexpr uint32_t kTimeLeftUnknown = 0xFFFFFFFF;
constexpr uint16_t kLapsLeftUnknown = 0xFFFF;
constexpr uint8_t kSettingNa = 0xFF;  // car has no ABS / TC / map
constexpr int16_t kFuelToFinishUnknown = 0x7FFF;

enum Flags : uint16_t {
  kFlagPitLimiter = 1 << 0,
  kFlagInPit = 1 << 1,
  kFlagLapInvalid = 1 << 2,
  kFlagEngineOn = 1 << 3,
  kFlagIgnitionOn = 1 << 4,
};

enum class Session : uint8_t { Unknown = 0, Practice = 1, Qualifying = 2, Race = 3, Hotlap = 4 };

enum class RaceFlag : uint8_t {
  None = 0, Green = 1, Yellow = 2, Blue = 3, White = 4, Black = 5, Chequered = 6, Red = 7,
  SafetyCar = 8, VirtualSafetyCar = 9  // VSC also covers full course yellow
};

// 0x10 TELEMETRY — fast, 20–30 Hz
struct Telemetry {
  uint32_t current_lap_ms = 0;
  uint32_t last_lap_ms = 0;
  uint32_t best_lap_ms = 0;
  uint32_t last_pit_ms = 0;
  int32_t delta_ms = 0;
  uint16_t lap_number = 0;
  uint8_t position = 0;
  int8_t gear = 0;
  uint16_t speed_kmh = 0;
  uint16_t fuel_laps_x10 = kFuelUnknown;
  uint16_t track_pos = 0;
  int16_t air_temp_x10 = 0;
  int16_t track_temp_x10 = 0;
  uint16_t flags = 0;
  Session session = Session::Unknown;
  RaceFlag race_flag = RaceFlag::None;

  bool has(Flags f) const { return (flags & f) != 0; }
};

// 0x12 ENGINE — fast, for shift lights
struct Engine {
  uint16_t rpm = 0;
  uint16_t shift_rpm = 0;  // 0 = unknown, no shift lights
  uint16_t max_rpm = 0;
};

enum class Compound : uint8_t { Unknown = 0, Soft = 1, Medium = 2, Hard = 3, Inter = 4, Wet = 5 };
enum Wheel : uint8_t { kFL = 0, kFR = 1, kRL = 2, kRR = 3 };

// 0x13 TYRES — ~5 Hz
struct Tyres {
  struct Corner {
    int16_t temp_x10 = 0;           // °C × 10
    uint16_t pressure_psi_x100 = 0;
    uint8_t wear_pct = 100;         // 100 = new
    int16_t brake_temp_c = 0;
  } wheel[4];
  int16_t opt_min_x10 = 0;  // optimal temperature window for the compound
  int16_t opt_max_x10 = 0;
  uint16_t target_psi_x100 = 0;
  Compound compound = Compound::Unknown;
};

// 0x14 CAR — ~5 Hz
struct Car {
  uint8_t abs = kSettingNa;
  uint8_t tc = kSettingNa;
  uint8_t tc_cut = kSettingNa;
  uint8_t engine_map = kSettingNa;
  uint16_t brake_bias_x10 = 0;  // % front × 10, 0 = unknown
  uint8_t damage_pct = 0;
};

// 0x15 FUEL — ~2 Hz
struct Fuel {
  uint16_t liters_x10 = 0;
  uint16_t per_lap_x100 = 0;  // average consumption, 0 = not known yet
  uint16_t capacity_x10 = 0;
  int16_t to_finish_x10 = kFuelToFinishUnknown;  // extra liters needed; <= 0 = enough
};

enum class Penalty : uint8_t { None = 0, DriveThrough = 1, StopGo = 2, TimePenalty = 3, Disqualified = 4 };
enum class PitWindow : uint8_t { Unknown = 0, Closed = 1, Open = 2 };
enum RaceBits : uint8_t { kRaceMandatoryStopDone = 1 << 0, kRaceLastLap = 1 << 1 };

// 0x16 RACE — ~5 Hz
struct Race {
  int32_t gap_ahead_ms = kGapUnknown;
  int32_t gap_behind_ms = kGapUnknown;
  uint32_t time_left_s = kTimeLeftUnknown;
  uint16_t laps_left = kLapsLeftUnknown;
  uint8_t car_count = 0;
  Penalty penalty = Penalty::None;
  uint8_t penalty_s = 0;
  uint8_t track_limits = 0;
  uint8_t track_limits_max = 0;  // 0 = no limit known
  PitWindow pit_window = PitWindow::Unknown;
  uint8_t bits = 0;
};

enum class SectorState : uint8_t { None = 0, Slower = 1, PersonalBest = 2, SessionBest = 3 };

// 0x17 TIMING — on sector change
struct Timing {
  uint8_t sector = 0;  // 0..2, current sector
  uint32_t sector_ms[3] = {0, 0, 0};
  uint32_t best_sector_ms[3] = {0, 0, 0};
  SectorState state[3] = {SectorState::None, SectorState::None, SectorState::None};
  uint32_t optimal_lap_ms = 0;
  uint32_t session_best_ms = 0;
};

// 0x18 WEATHER — ~1 Hz. Rain levels: 0 dry … 5 storm.
struct Weather {
  uint8_t rain = 0;
  uint8_t rain_in_10 = 0;
  uint8_t rain_in_30 = 0;
  uint8_t wetness = 0;
  uint8_t grip_pct = 100;
  uint8_t wind_kmh = 0;
  uint16_t wind_dir_deg = 0;
  uint8_t clouds_pct = 0;
};

// 0x19 LAP_HISTORY — on lap completion
struct LapHistory {
  uint8_t count = 0;
  uint32_t lap_ms[5] = {0, 0, 0, 0, 0};  // newest first
  uint8_t valid_mask = 0;                // bit i = lap_ms[i] valid
  bool valid(int i) const { return (valid_mask >> i) & 1; }
};

enum Units : uint8_t { kUnitsMph = 1 << 0, kUnitsFahrenheit = 1 << 1, kUnitsBar = 1 << 2 };

// 0x20 CONFIG — sent by the bridge on connect
struct Config {
  uint16_t page_mask = 0xFFFF;  // bit i = page i enabled (Screen order)
  uint8_t brightness = 200;
  uint8_t units = 0;
};

struct State {
  Telemetry telemetry;
  Engine engine;
  Tyres tyres;
  Car car;
  Fuel fuel;
  Race race;
  Timing timing;
  Weather weather;
  LapHistory history;
  Config config;
  char track_name[49] = "";
  // Which messages arrived at least once.
  bool has_engine = false, has_tyres = false, has_car = false, has_fuel = false;
  bool has_race = false, has_timing = false, has_weather = false, has_history = false;
};

}  // namespace rigdash
