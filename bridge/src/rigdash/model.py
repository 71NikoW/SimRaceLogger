"""Game-agnostic telemetry model. Every game adapter produces this."""

from dataclasses import dataclass
from enum import IntEnum, IntFlag

FUEL_UNKNOWN = 0xFFFF


class Session(IntEnum):
    UNKNOWN = 0
    PRACTICE = 1
    QUALIFYING = 2
    RACE = 3
    HOTLAP = 4


class RaceFlag(IntEnum):
    NONE = 0
    GREEN = 1
    YELLOW = 2
    BLUE = 3
    WHITE = 4
    BLACK = 5
    CHEQUERED = 6
    RED = 7
    SAFETY_CAR = 8
    VIRTUAL_SAFETY_CAR = 9  # also full course yellow / slow zones


class Flag(IntFlag):
    NONE = 0
    PIT_LIMITER = 1 << 0
    IN_PIT = 1 << 1
    LAP_INVALID = 1 << 2
    ENGINE_ON = 1 << 3
    IGNITION_ON = 1 << 4


@dataclass
class Telemetry:
    current_lap_ms: int = 0
    last_lap_ms: int = 0
    best_lap_ms: int = 0
    last_pit_ms: int = 0
    delta_ms: int = 0
    lap_number: int = 0
    position: int = 0
    gear: int = 0
    speed_kmh: int = 0
    fuel_laps_x10: int = FUEL_UNKNOWN
    track_pos: int = 0
    air_temp_x10: int = 0
    track_temp_x10: int = 0
    flags: Flag = Flag.NONE
    session: Session = Session.UNKNOWN
    race_flag: RaceFlag = RaceFlag.NONE
