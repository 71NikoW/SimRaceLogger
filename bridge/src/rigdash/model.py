"""Game-agnostic telemetry model. Every game adapter produces this."""

from dataclasses import dataclass, field
from typing import Optional
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


# --- Slower messages (docs/PROTOCOL.md 0x12..0x20) ---------------------------

GAP_UNKNOWN = 0x7FFFFFFF
TIME_LEFT_UNKNOWN = 0xFFFFFFFF
LAPS_LEFT_UNKNOWN = 0xFFFF
SETTING_NA = 0xFF
FUEL_TO_FINISH_UNKNOWN = 0x7FFF


class Compound(IntEnum):
    UNKNOWN = 0
    SOFT = 1
    MEDIUM = 2
    HARD = 3
    INTER = 4
    WET = 5


class Penalty(IntEnum):
    NONE = 0
    DRIVE_THROUGH = 1
    STOP_GO = 2
    TIME_PENALTY = 3
    DISQUALIFIED = 4


class PitWindow(IntEnum):
    UNKNOWN = 0
    CLOSED = 1
    OPEN = 2


class RaceBits(IntFlag):
    NONE = 0
    MANDATORY_STOP_DONE = 1 << 0
    LAST_LAP = 1 << 1


class SectorState(IntEnum):
    NONE = 0
    SLOWER = 1
    PERSONAL_BEST = 2
    SESSION_BEST = 3


class Units(IntFlag):
    METRIC = 0
    MPH = 1 << 0
    FAHRENHEIT = 1 << 1
    BAR = 1 << 2


@dataclass
class Engine:
    rpm: int = 0
    shift_rpm: int = 0
    max_rpm: int = 0


@dataclass
class Corner:
    temp_x10: int = 0
    pressure_psi_x100: int = 0
    wear_pct: int = 100
    brake_temp_c: int = 0


def _four_corners() -> list[Corner]:
    return [Corner() for _ in range(4)]


@dataclass
class Tyres:
    wheels: list[Corner] = field(default_factory=_four_corners)  # FL, FR, RL, RR
    opt_min_x10: int = 0
    opt_max_x10: int = 0
    target_psi_x100: int = 0
    compound: Compound = Compound.UNKNOWN


@dataclass
class Car:
    abs: int = SETTING_NA
    tc: int = SETTING_NA
    tc_cut: int = SETTING_NA
    engine_map: int = SETTING_NA
    brake_bias_x10: int = 0
    damage_pct: int = 0


@dataclass
class Fuel:
    liters_x10: int = 0
    per_lap_x100: int = 0
    capacity_x10: int = 0
    to_finish_x10: int = FUEL_TO_FINISH_UNKNOWN


@dataclass
class Race:
    gap_ahead_ms: int = GAP_UNKNOWN
    gap_behind_ms: int = GAP_UNKNOWN
    time_left_s: int = TIME_LEFT_UNKNOWN
    laps_left: int = LAPS_LEFT_UNKNOWN
    car_count: int = 0
    penalty: Penalty = Penalty.NONE
    penalty_s: int = 0
    track_limits: int = 0
    track_limits_max: int = 0
    pit_window: PitWindow = PitWindow.UNKNOWN
    bits: RaceBits = RaceBits.NONE


def _three_zeros() -> list[int]:
    return [0, 0, 0]


def _three_states() -> list[SectorState]:
    return [SectorState.NONE] * 3


@dataclass
class Timing:
    sector: int = 0
    sector_ms: list[int] = field(default_factory=_three_zeros)
    best_sector_ms: list[int] = field(default_factory=_three_zeros)
    state: list[SectorState] = field(default_factory=_three_states)
    optimal_lap_ms: int = 0
    session_best_ms: int = 0


@dataclass
class Weather:
    """Rain levels: 0 dry, 1 drizzle, 2 light, 3 medium, 4 heavy, 5 storm."""
    rain: int = 0
    rain_in_10: int = 0
    rain_in_30: int = 0
    wetness: int = 0
    grip_pct: int = 100
    wind_kmh: int = 0
    wind_dir_deg: int = 0
    clouds_pct: int = 0


@dataclass
class LapHistory:
    laps: list[tuple[int, bool]] = field(default_factory=list)  # (lap_ms, valid), newest first, max 5


@dataclass
class Config:
    page_mask: int = 0xFF
    brightness: int = 200
    units: Units = Units.METRIC


@dataclass
class Snapshot:
    """Everything a source knows at one moment. Unknown parts stay None."""
    telemetry: Telemetry
    engine: Optional[Engine] = None
    tyres: Optional[Tyres] = None
    car: Optional[Car] = None
    fuel: Optional[Fuel] = None
    race: Optional[Race] = None
    timing: Optional[Timing] = None
    weather: Optional[Weather] = None
    history: Optional[LapHistory] = None
