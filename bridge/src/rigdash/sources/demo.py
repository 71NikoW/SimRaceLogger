"""Fake driver lapping a track — for testing the display without a sim."""

import random
from typing import Optional

from ..model import Flag, RaceFlag, Session, Telemetry
from .base import TelemetrySource


class DemoSource(TelemetrySource):
    PIT_EXIT_MS = 15_000  # first 15 s of a session: driving out of the pits

    def __init__(self, lap_ms: int = 88_200, seed: int = 1, fuel_laps: float = 12.0,
                 fuel_per_lap: float = 1.0, invalid_chance: float = 0.15) -> None:
        self.lap_ms = lap_ms
        self.fuel_laps = fuel_laps
        self.fuel_per_lap = fuel_per_lap
        self.invalid_chance = invalid_chance
        self._rng = random.Random(seed)
        self._start_ms: Optional[int] = None
        self._lap_start_ms = 0
        self._lap_target_ms = lap_ms
        self._lap = 1
        self._last = 0
        self._best = 0
        self._invalid_at: Optional[int] = None

    def track_name(self) -> str:
        return "Demo Ring"

    def _new_lap(self, start_ms: int) -> None:
        self._lap_start_ms = start_ms
        self._lap_target_ms = self.lap_ms + self._rng.randint(-1500, 1500)
        self._invalid_at = (self._rng.randint(10, 90) * self._lap_target_ms // 100
                            if self._rng.random() < self.invalid_chance else None)

    def poll(self, now_ms: int) -> Optional[Telemetry]:
        if self._start_ms is None:
            self._start_ms = now_ms
            self._new_lap(now_ms)
        while now_ms - self._lap_start_ms >= self._lap_target_ms:
            invalid = self._invalid_at is not None
            self._last = self._lap_target_ms
            if not invalid and (self._best == 0 or self._last < self._best):
                self._best = self._last
            self._lap += 1
            self.fuel_laps = max(0.0, self.fuel_laps - self.fuel_per_lap)
            self._new_lap(self._lap_start_ms + self._lap_target_ms)

        cur = now_ms - self._lap_start_ms
        in_pit = now_ms - self._start_ms < self.PIT_EXIT_MS
        flags = Flag.ENGINE_ON | Flag.IGNITION_ON
        if in_pit:
            flags |= Flag.PIT_LIMITER | Flag.IN_PIT
        if self._invalid_at is not None and cur >= self._invalid_at:
            flags |= Flag.LAP_INVALID
        fraction = cur / self._lap_target_ms
        delta = 0
        if self._best:
            delta = int(cur - fraction * self._best)
        return Telemetry(
            current_lap_ms=cur, last_lap_ms=self._last, best_lap_ms=self._best,
            delta_ms=delta, lap_number=self._lap, position=1,
            gear=2 if in_pit else 1 + int(fraction * 50) % 6,
            speed_kmh=60 if in_pit else 120 + int(fraction * 1000) % 140,
            fuel_laps_x10=round(self.fuel_laps * 10), track_pos=int(fraction * 10000),
            air_temp_x10=261, track_temp_x10=372, flags=flags, session=Session.PRACTICE,
            race_flag=RaceFlag.GREEN,
        )
