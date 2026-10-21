"""Session logger: one CSV row per completed lap."""

import csv
import datetime as _dt
from pathlib import Path
from typing import Optional, TextIO

from .model import Flag, Snapshot

FIELDS = [
    "timestamp", "track", "lap", "lap_ms", "lap_time", "valid", "s1_ms", "s2_ms", "s3_ms",
    "fuel_used_l", "fuel_left_l", "position", "air_c", "track_c", "rain", "race_flag",
]


def fmt_lap(ms: int) -> str:
    return f"{ms // 60000}:{ms // 1000 % 60:02d}.{ms % 1000:03d}" if ms else ""


class SessionLogger:
    def __init__(self, path: Path | str, track: str = "", clock=None) -> None:
        self.path = Path(path)
        self.track = track
        self._clock = clock or (lambda: _dt.datetime.now().isoformat(timespec="seconds"))
        new = not self.path.exists() or self.path.stat().st_size == 0
        self._fh: TextIO = self.path.open("a", newline="", encoding="utf-8")
        self._csv = csv.DictWriter(self._fh, fieldnames=FIELDS)
        if new:
            self._csv.writeheader()
        self._lap: Optional[int] = None
        self._invalid = False
        self._sectors = [0, 0]
        self._fuel_start: Optional[float] = None
        self.rows = 0

    def update(self, snap: Snapshot) -> bool:
        """Feed every frame. Returns True when a row was written."""
        t = snap.telemetry
        fuel = snap.fuel.liters_x10 / 10 if snap.fuel else None
        wrote = False
        if self._lap is not None and t.lap_number == self._lap + 1 and t.last_lap_ms:
            s1, s2 = self._sectors
            s3 = t.last_lap_ms - s1 - s2 if s1 and s2 else 0
            used = (self._fuel_start - fuel) if fuel is not None and self._fuel_start is not None else None
            self._csv.writerow({
                "timestamp": self._clock(), "track": self.track, "lap": self._lap,
                "lap_ms": t.last_lap_ms, "lap_time": fmt_lap(t.last_lap_ms),
                "valid": int(not self._invalid), "s1_ms": s1 or "", "s2_ms": s2 or "",
                "s3_ms": s3 or "",
                "fuel_used_l": f"{used:.2f}" if used is not None and used >= 0 else "",
                "fuel_left_l": f"{fuel:.1f}" if fuel is not None else "",
                "position": t.position or "", "air_c": t.air_temp_x10 / 10,
                "track_c": t.track_temp_x10 / 10,
                "rain": snap.weather.rain if snap.weather else "",
                "race_flag": int(t.race_flag),
            })
            self._fh.flush()
            self.rows += 1
            wrote = True
        if t.lap_number != self._lap:
            self._lap, self._invalid, self._sectors, self._fuel_start = t.lap_number, False, [0, 0], fuel
        self._invalid = self._invalid or bool(t.flags & Flag.LAP_INVALID)
        if snap.timing:
            for i in (0, 1):
                if snap.timing.sector_ms[i]:
                    self._sectors[i] = snap.timing.sector_ms[i]
        return wrote

    def close(self) -> None:
        self._fh.close()
