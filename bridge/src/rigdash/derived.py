"""Values most games don't provide directly, computed from the raw stream.

All trackers are fed every frame and detect lap changes themselves, so a game
adapter only has to report what the game exposes.
"""

import math
from collections import deque
from typing import Optional

from .model import FUEL_TO_FINISH_UNKNOWN, LapHistory, SectorState, Timing


class FuelCalculator:
    """Average fuel per lap over the last `window` clean laps."""

    def __init__(self, window: int = 5) -> None:
        self._usage: deque[float] = deque(maxlen=window)
        self._lap: Optional[int] = None
        self._fuel_at_start = 0.0
        self._dirty = False  # pitted / refuelled during this lap

    def update(self, lap_number: int, fuel_l: float, in_pit: bool = False) -> None:
        if self._lap is None:
            self._lap, self._fuel_at_start = lap_number, fuel_l
            self._dirty = True  # first lap seen is partial
            return
        if in_pit or fuel_l > self._fuel_at_start + 0.05:
            self._dirty = True
        if lap_number != self._lap:
            used = self._fuel_at_start - fuel_l
            if not self._dirty and used > 0 and lap_number == self._lap + 1:
                self._usage.append(used)
            self._lap, self._fuel_at_start, self._dirty = lap_number, fuel_l, in_pit

    @property
    def per_lap(self) -> Optional[float]:
        return sum(self._usage) / len(self._usage) if self._usage else None

    def laps_remaining(self, fuel_l: float) -> Optional[float]:
        per = self.per_lap
        return fuel_l / per if per else None

    def to_finish(self, fuel_l: float, laps_left: Optional[int] = None,
                  time_left_s: Optional[float] = None, lap_ms: Optional[int] = None,
                  margin_laps: float = 0.5) -> Optional[float]:
        """Extra liters needed to finish (<= 0: enough). None if unknown.

        Timed races: laps = ceil(time left / lap time) + 1, because the lap
        running when the clock hits zero is still completed.
        """
        per = self.per_lap
        if not per:
            return None
        if laps_left is None:
            if time_left_s is None or not lap_ms:
                return None
            laps_left = math.ceil(time_left_s * 1000 / lap_ms) + 1
        return (laps_left + margin_laps) * per - fuel_l

    @staticmethod
    def encode_to_finish(liters: Optional[float]) -> int:
        if liters is None:
            return FUEL_TO_FINISH_UNKNOWN
        return max(-0x7FFE, min(0x7FFE, round(liters * 10)))


class SectorTracker:
    """Sector times from track position. Boundaries are lap fractions."""

    def __init__(self, boundaries: tuple[float, float] = (1 / 3, 2 / 3)) -> None:
        self.bounds = boundaries
        self._lap: Optional[int] = None
        self._sector = 0
        self._sector_start_ms = 0
        self._times = [0, 0, 0]
        self._states = [SectorState.NONE] * 3
        self._valid = True
        self.best = [0, 0, 0]          # personal best per sector (valid laps)
        self.session_best = [0, 0, 0]  # can be raised by other drivers via note_session_best
        self.session_best_lap = 0

    def note_session_best(self, sector: int, ms: int) -> None:
        if ms and (not self.session_best[sector] or ms < self.session_best[sector]):
            self.session_best[sector] = ms

    def _sector_of(self, frac: float) -> int:
        return 0 if frac < self.bounds[0] else 1 if frac < self.bounds[1] else 2

    def _close(self, sector: int, ms: int) -> None:
        self._times[sector] = ms
        if not self._valid:
            self._states[sector] = SectorState.SLOWER
            return
        sb, pb = self.session_best[sector], self.best[sector]
        if not sb or ms < sb:
            self.session_best[sector] = ms
            self._states[sector] = SectorState.SESSION_BEST
        elif ms == sb:
            self._states[sector] = SectorState.SESSION_BEST
        elif not pb or ms < pb:
            self._states[sector] = SectorState.PERSONAL_BEST
        else:
            self._states[sector] = SectorState.SLOWER
        if not pb or ms < pb:
            self.best[sector] = ms

    def update(self, lap_number: int, track_pos: int, current_lap_ms: int, lap_invalid: bool,
               last_lap_ms: int = 0) -> None:
        frac = track_pos / 10000
        if self._lap is None:
            self._lap, self._sector = lap_number, self._sector_of(frac)
            self._sector_start_ms = current_lap_ms
            self._valid = False  # joined mid-lap
            return
        if lap_number != self._lap:
            if self._sector == 2 and last_lap_ms:
                self._close(2, last_lap_ms - self._sector_start_ms)
                if self._valid and (not self.session_best_lap or last_lap_ms < self.session_best_lap):
                    self.session_best_lap = last_lap_ms
            self._lap, self._sector, self._sector_start_ms = lap_number, 0, 0
            self._times, self._valid = [0, 0, 0], not lap_invalid
            # Keep last lap's colours for S2/S3 until they are driven again.
            self._states[0] = SectorState.NONE
            return
        if lap_invalid:
            self._valid = False
        sector = self._sector_of(frac)
        if sector > self._sector:
            self._close(self._sector, current_lap_ms - self._sector_start_ms)
            for skipped in range(self._sector + 1, sector):  # very low sample rate
                self._states[skipped] = SectorState.NONE
            self._sector, self._sector_start_ms = sector, current_lap_ms
            if sector == 1:
                self._states[1] = self._states[2] = SectorState.NONE

    def timing(self) -> Timing:
        optimal = sum(self.best) if all(self.best) else 0
        return Timing(self._sector, list(self._times), list(self.best), list(self._states),
                      optimal, self.session_best_lap)


class LapHistoryTracker:
    def __init__(self) -> None:
        self._lap: Optional[int] = None
        self._invalid = False
        self.laps: deque[tuple[int, bool]] = deque(maxlen=5)

    def update(self, lap_number: int, last_lap_ms: int, lap_invalid: bool) -> bool:
        """Returns True when a lap was just added."""
        added = False
        if self._lap is not None and lap_number == self._lap + 1 and last_lap_ms:
            self.laps.appendleft((last_lap_ms, not self._invalid))
            added = True
        if lap_number != self._lap:
            self._invalid = False
        self._lap = lap_number
        self._invalid = self._invalid or lap_invalid
        return added

    def history(self) -> LapHistory:
        return LapHistory(list(self.laps))


class Enricher:
    """Fills in what the source left out: fuel stats, sectors, lap history."""

    def __init__(self) -> None:
        self.fuel = FuelCalculator()
        self.sectors = SectorTracker()
        self.laps = LapHistoryTracker()

    def apply(self, snap):  # Snapshot -> Snapshot (same object, filled in)
        from .model import Flag, LAPS_LEFT_UNKNOWN, TIME_LEFT_UNKNOWN

        t = snap.telemetry
        invalid = bool(t.flags & Flag.LAP_INVALID)
        in_pit = bool(t.flags & Flag.IN_PIT)

        if snap.fuel is not None:
            liters = snap.fuel.liters_x10 / 10
            self.fuel.update(t.lap_number, liters, in_pit)
            per = self.fuel.per_lap
            if per and not snap.fuel.per_lap_x100:
                snap.fuel.per_lap_x100 = round(per * 100)
            if per and t.fuel_laps_x10 == 0xFFFF:
                t.fuel_laps_x10 = min(0xFFFE, round(liters / per * 10))
            if per and snap.fuel.to_finish_x10 == FUEL_TO_FINISH_UNKNOWN and snap.race is not None:
                r = snap.race
                laps_left = None if r.laps_left == LAPS_LEFT_UNKNOWN else r.laps_left
                time_left = None if r.time_left_s == TIME_LEFT_UNKNOWN else r.time_left_s
                lap_ms = t.best_lap_ms or t.last_lap_ms
                snap.fuel.to_finish_x10 = self.fuel.encode_to_finish(
                    self.fuel.to_finish(liters, laps_left, time_left, lap_ms))

        if snap.timing is None:
            self.sectors.update(t.lap_number, t.track_pos, t.current_lap_ms, invalid, t.last_lap_ms)
            snap.timing = self.sectors.timing()

        self.laps.update(t.lap_number, t.last_lap_ms, invalid)
        if snap.history is None:
            snap.history = self.laps.history()
        return snap
