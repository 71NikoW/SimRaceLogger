"""Fake driver lapping a track — for testing the display without a sim.

Covers every message: shift lights, tyres warming up, fuel, gaps, and a
weather cycle (dry → rain forecast → rain → heavy → drying) so all warnings
can be seen.
"""

import math
import random
from typing import Optional

from ..model import (
    Car, Compound, Corner, Engine, Flag, Fuel, PitWindow, Race, RaceFlag, Session, Snapshot,
    Telemetry, Tyres, Weather,
)
from .base import TelemetrySource

# (start_s, rain, rain_in_10, rain_in_30, wetness) — repeats every cycle.
WEATHER_CYCLE_S = 16 * 60
WEATHER_PLAN = [
    (0, 0, 0, 0, 0),
    (3 * 60, 0, 0, 2, 0),
    (4 * 60, 0, 2, 3, 0),
    (6 * 60, 2, 3, 3, 1),
    (8 * 60, 4, 3, 1, 3),
    (11 * 60, 1, 0, 0, 3),
    (12 * 60, 0, 0, 0, 2),
    (14 * 60, 0, 0, 0, 1),
]


def weather_at(t_s: float) -> tuple[int, int, int, int]:
    t = t_s % WEATHER_CYCLE_S
    current = WEATHER_PLAN[0]
    for step in WEATHER_PLAN:
        if t >= step[0]:
            current = step
    return current[1:]


class DemoSource(TelemetrySource):
    PIT_EXIT_MS = 15_000  # first 15 s of a session: driving out of the pits
    TANK_L = 100.0
    SHIFT_RPM = 7800
    MAX_RPM = 8300
    RACE_S = 30 * 60

    def __init__(self, lap_ms: int = 88_200, seed: int = 1, fuel_laps: float = 12.0,
                 fuel_per_lap: float = 2.8, invalid_chance: float = 0.15) -> None:
        self.lap_ms = lap_ms
        self.fuel_per_lap = fuel_per_lap
        self.fuel_l = fuel_laps * fuel_per_lap
        self.invalid_chance = invalid_chance
        self._rng = random.Random(seed)
        self._start_ms: Optional[int] = None
        self._lap_start_ms = 0
        self._lap_target_ms = lap_ms
        self._lap = 1
        self._last = 0
        self._best = 0
        self._invalid_at: Optional[int] = None
        self._limits = 0
        self._last_t = 0

    @property
    def fuel_laps(self) -> float:
        return self.fuel_l / self.fuel_per_lap

    def track_name(self) -> str:
        return "Demo Ring"

    def _new_lap(self, start_ms: int) -> None:
        self._lap_start_ms = start_ms
        self._lap_target_ms = self.lap_ms + self._rng.randint(-1500, 1500)
        self._invalid_at = (self._rng.randint(10, 90) * self._lap_target_ms // 100
                            if self._rng.random() < self.invalid_chance else None)

    def poll(self, now_ms: int) -> Optional[Snapshot]:
        if self._start_ms is None:
            self._start_ms = now_ms
            self._last_t = now_ms
            self._new_lap(now_ms)
        while now_ms - self._lap_start_ms >= self._lap_target_ms:
            invalid = self._invalid_at is not None
            self._last = self._lap_target_ms
            if invalid:
                self._limits += 1
            if not invalid and (self._best == 0 or self._last < self._best):
                self._best = self._last
            self._lap += 1
            self._new_lap(self._lap_start_ms + self._lap_target_ms)

        elapsed_s = (now_ms - self._start_ms) / 1000
        cur = now_ms - self._lap_start_ms
        in_pit = now_ms - self._start_ms < self.PIT_EXIT_MS
        fraction = cur / self._lap_target_ms
        dt = (now_ms - self._last_t) / 1000
        self._last_t = now_ms
        if not in_pit:
            self.fuel_l = max(0.0, self.fuel_l - self.fuel_per_lap * dt * 1000 / self._lap_target_ms)

        flags = Flag.ENGINE_ON | Flag.IGNITION_ON
        if in_pit:
            flags |= Flag.PIT_LIMITER | Flag.IN_PIT
        if self._invalid_at is not None and cur >= self._invalid_at:
            flags |= Flag.LAP_INVALID
        delta = int(cur - fraction * self._best) if self._best else 0

        gear = 2 if in_pit else 1 + int(fraction * 50) % 6
        sub = (fraction * 50) % 1  # position within the current gear
        rpm = 3200 if in_pit else int(4800 + sub * (self.SHIFT_RPM + 300 - 4800))
        speed = 60 if in_pit else 90 + gear * 35 + int(sub * 30)
        rain, rain10, rain30, wet = weather_at(elapsed_s)
        warm = min(1.0, elapsed_s / 240)  # tyres take ~4 min to come in

        t = Telemetry(
            current_lap_ms=cur, last_lap_ms=self._last, best_lap_ms=self._best, delta_ms=delta,
            lap_number=self._lap, position=3, gear=gear, speed_kmh=speed,
            fuel_laps_x10=round(self.fuel_laps * 10), track_pos=int(fraction * 10000),
            air_temp_x10=261 - rain * 15, track_temp_x10=372 - wet * 40, flags=flags,
            session=Session.RACE, race_flag=RaceFlag.GREEN,
        )
        wheels = []
        for i in range(4):
            front = i < 2
            base = 45 + warm * (40 if front else 36) - wet * 6
            wobble = 4 * math.sin(fraction * 2 * math.pi * 3 + i)
            wheels.append(Corner(
                temp_x10=round((base + wobble) * 10),
                pressure_psi_x100=round((22.5 + warm * 4.8 + (0.3 if front else 0)) * 100),
                wear_pct=max(0, 100 - int(elapsed_s / 60 * (1.4 if front else 1.1))),
                brake_temp_c=int(220 + warm * 250 + 180 * abs(math.sin(fraction * 2 * math.pi * 4))),
            ))
        left_s = max(0, self.RACE_S - int(elapsed_s))
        return Snapshot(
            telemetry=t,
            engine=Engine(rpm, self.SHIFT_RPM, self.MAX_RPM),
            tyres=Tyres(wheels, 800, 950, 2750, Compound.MEDIUM),
            car=Car(abs=3, tc=4, tc_cut=2, engine_map=1, brake_bias_x10=565,
                    damage_pct=min(100, self._limits * 2)),
            fuel=Fuel(liters_x10=round(self.fuel_l * 10), capacity_x10=round(self.TANK_L * 10)),
            race=Race(
                gap_ahead_ms=1800 + int(600 * math.sin(elapsed_s / 40)),
                gap_behind_ms=-(2500 + int(900 * math.cos(elapsed_s / 55))),
                time_left_s=left_s, car_count=24, track_limits=self._limits, track_limits_max=4,
                pit_window=PitWindow.OPEN if 10 * 60 <= elapsed_s <= 20 * 60 else PitWindow.CLOSED,
            ),
            weather=Weather(rain, rain10, rain30, wet, grip_pct=100 - wet * 8,
                            wind_kmh=12, wind_dir_deg=225, clouds_pct=min(100, 30 + rain30 * 20)),
        )
