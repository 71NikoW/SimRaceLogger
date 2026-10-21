#pragma once
#include <stdint.h>

namespace rigdash {

constexpr uint16_t kFuelUnknown = 0xFFFF;

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

}  // namespace rigdash
