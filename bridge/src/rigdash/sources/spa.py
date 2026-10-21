"""Scripted race at Spa-Francorchamps for demos.

Six laps with pit exit, a track-limits lap, local yellow at Pouhon, blue flag
at Blanchimont, a safety-car lap with rain arriving, a last lap and the
chequered flag — then it starts over. Same data drives the browser simulator
(simulator/index.html embeds tracks/spa.json).
"""

import bisect
import json
import math
from importlib import resources
from typing import Optional

from ..model import (
    Car, Compound, Corner, Engine, Flag, Fuel, PitWindow, Race, RaceBits, RaceFlag, Session,
    Snapshot, Telemetry, Tyres, Weather,
)
from .base import TelemetrySource

FLAG_NAMES = {
    "green": RaceFlag.GREEN, "yellow": RaceFlag.YELLOW, "blue": RaceFlag.BLUE,
    "safety_car": RaceFlag.SAFETY_CAR, "vsc": RaceFlag.VIRTUAL_SAFETY_CAR,
    "chequered": RaceFlag.CHEQUERED, "red": RaceFlag.RED, "white": RaceFlag.WHITE,
}
WEATHER = {  # rain, rain_in_10, rain_in_30, wetness
    "dry": (0, 0, 0, 0), "soon": (0, 2, 3, 0), "light": (2, 2, 3, 1),
    "heavy": (4, 4, 3, 3), "drying": (0, 0, 0, 2),
}
SC_SPEED_KMH = 150
COOL_DOWN_FRAC = 0.15  # part of the lap after the finish before the race restarts
# Speed (km/h) at 8000 rpm per gear — picks gear and rpm from speed.
GEAR_TOP = [95, 130, 165, 200, 240, 290]
SHIFT_RPM, MAX_RPM, REDLINE_RPM = 7800, 8300, 8000
FUEL_PER_LAP = 3.6


def load_track(name: str = "spa") -> dict:
    return json.loads(resources.files("rigdash.tracks").joinpath(f"{name}.json").read_text())


class LapProfile:
    """Time ↔ distance along one lap from the speed profile."""

    STEP_M = 5.0

    def __init__(self, track: dict, speed_cap: Optional[float] = None, scale: Optional[float] = None):
        self.length = track["length_m"]
        pts = track["profile"]
        xs, vs = [p[0] for p in pts], [p[1] for p in pts]
        self.dist = [0.0]
        self.time = [0.0]
        d = 0.0
        while d < self.length:
            v = self.speed_at(xs, vs, d + self.STEP_M / 2)
            if speed_cap:
                v = min(v, speed_cap)
            self.time.append(self.time[-1] + self.STEP_M / (v / 3.6))
            d = min(self.length, d + self.STEP_M)
            self.dist.append(d)
        # Scale so a normal lap matches target_lap_ms (profile is approximate).
        self.scale = scale if scale is not None else track["target_lap_ms"] / 1000 / self.time[-1]
        self.time = [t * self.scale for t in self.time]
        self._xs, self._vs, self._cap = xs, vs, speed_cap

    @staticmethod
    def speed_at(xs, vs, d: float) -> float:
        i = max(1, min(len(xs) - 1, bisect.bisect_right(xs, d)))
        x0, x1, v0, v1 = xs[i - 1], xs[i], vs[i - 1], vs[i]
        return v0 + (v1 - v0) * (d - x0) / (x1 - x0) if x1 > x0 else v0

    @property
    def lap_ms(self) -> int:
        return round(self.time[-1] * 1000)

    def distance_at(self, t_ms: float) -> float:
        t = t_ms / 1000
        i = max(1, min(len(self.time) - 1, bisect.bisect_right(self.time, t)))
        t0, t1 = self.time[i - 1], self.time[i]
        return self.dist[i - 1] + (self.dist[i] - self.dist[i - 1]) * (t - t0) / (t1 - t0)

    def time_at(self, d: float) -> float:
        """ms to reach distance d."""
        i = max(1, min(len(self.dist) - 1, bisect.bisect_right(self.dist, d)))
        d0, d1 = self.dist[i - 1], self.dist[i]
        return 1000 * (self.time[i - 1] + (self.time[i] - self.time[i - 1]) * (d - d0) / (d1 - d0))

    def speed(self, d: float) -> float:
        v = self.speed_at(self._xs, self._vs, d) / self.scale
        return min(v, self._cap / self.scale) if self._cap else v


def gear_for(speed: float) -> int:
    for g, top in enumerate(GEAR_TOP, start=1):
        if speed <= top * SHIFT_RPM / REDLINE_RPM:
            return g
    return len(GEAR_TOP)


def rpm_for(speed: float, gear: int) -> int:
    return min(MAX_RPM, max(3000, round(speed / GEAR_TOP[gear - 1] * REDLINE_RPM)))


class SpaSource(TelemetrySource):
    def __init__(self, track: Optional[dict] = None) -> None:
        self.track = track or load_track("spa")
        self.normal = LapProfile(self.track)
        self.sc = LapProfile(self.track, speed_cap=SC_SPEED_KMH, scale=self.normal.scale)
        self.script = self.track["script"]
        self._start: Optional[int] = None
        self._lap = 1
        self._lap_start = 0
        self._last = 0
        self._best = 0
        self._invalid = False
        self._fuel = 40.0
        self._limits = 0
        self._position = 3

    def track_name(self) -> str:
        return self.track["name"]

    def events(self, lap: int, frac: float) -> dict:
        """Merged script state at this point of the race."""
        out = {"flag": "green", "weather": "dry", "mode": "normal"}
        for e in self.script["events"]:
            if e["lap"] == lap and e["from"] <= frac < e["to"] + 1e-9:
                out.update({k: v for k, v in e.items() if k not in ("lap", "from", "to")})
        if lap == 1 and frac < self.track["pit_exit"]:
            out["pit"] = True
        return out

    def _profile(self, lap: int) -> LapProfile:
        sc = any(e["lap"] == lap and e.get("mode") == "sc" for e in self.script["events"])
        return self.sc if sc else self.normal

    def corner_at(self, d: float) -> str:
        name = ""
        for at, n in self.track["corners"]:
            if d >= at:
                name = n
        return name

    def poll(self, now_ms: int) -> Optional[Snapshot]:
        if self._start is None:
            self._start = now_ms
            self._lap_start = now_ms
        laps = self.script["laps"]
        prof = self._profile(self._lap)
        cur = now_ms - self._lap_start
        while cur >= prof.lap_ms:
            self._last = prof.lap_ms
            if not self._invalid and (not self._best or self._last < self._best):
                self._best = self._last
            self._lap += 1
            self._lap_start += prof.lap_ms
            self._invalid = False
            self._fuel -= FUEL_PER_LAP
            prof = self._profile(self._lap)
            cur = now_ms - self._lap_start

        d = prof.distance_at(cur)
        frac = d / self.track["length_m"]
        if self._lap > laps and frac >= COOL_DOWN_FRAC:  # chequered shown: start over
            self.__init__(self.track)
            return self.poll(now_ms)
        ev = self.events(self._lap, frac)
        if ev.get("invalid") and not self._invalid:
            self._invalid = True
            self._limits += 1
        if "position" in ev:
            self._position = ev["position"]

        pit = bool(ev.get("pit"))
        speed = 60.0 if pit else prof.speed(d)
        gear = 2 if pit else gear_for(speed)
        rain, rain10, rain30, wet = WEATHER[ev["weather"]]
        flags = Flag.ENGINE_ON | Flag.IGNITION_ON
        if pit:
            flags |= Flag.PIT_LIMITER | Flag.IN_PIT
        if self._invalid:
            flags |= Flag.LAP_INVALID
        fuel_now = max(0.0, self._fuel - FUEL_PER_LAP * frac)
        delta = 0
        if self._best:
            delta = round(cur - self.normal.time_at(d) * self._best / self.normal.lap_ms)
        race_bits = RaceBits.LAST_LAP if ev.get("last_lap") else RaceBits.NONE
        elapsed_s = (now_ms - self._start) / 1000
        t = Telemetry(
            current_lap_ms=round(cur), last_lap_ms=self._last, best_lap_ms=self._best,
            delta_ms=delta, lap_number=self._lap, position=self._position, gear=gear,
            speed_kmh=round(speed), fuel_laps_x10=round(fuel_now / FUEL_PER_LAP * 10),
            track_pos=min(10000, int(frac * 10000)), air_temp_x10=184 - rain * 12,
            track_temp_x10=243 - wet * 30, flags=flags, session=Session.RACE,
            race_flag=FLAG_NAMES[ev["flag"]],
        )
        warm = min(1.0, 0.6 + elapsed_s / 300)
        wheels = [Corner(
            temp_x10=round((55 + warm * 33 + (3 if i == 1 else 0) - wet * 5
                            + 4 * math.sin(frac * 2 * math.pi * 5 + i)) * 10),
            pressure_psi_x100=round((25.0 + warm * 2.6) * 100),
            wear_pct=100 - int(self._lap * 1.5), brake_temp_c=round(250 + 400 * max(0.0, 1 - speed / 200)),
        ) for i in range(4)]
        return Snapshot(
            telemetry=t,
            engine=Engine(rpm_for(speed, gear), SHIFT_RPM, MAX_RPM),
            tyres=Tyres(wheels, 800, 950, 2750, Compound.MEDIUM),
            car=Car(abs=2, tc=3, tc_cut=1, engine_map=1, brake_bias_x10=548,
                    damage_pct=0),
            fuel=Fuel(liters_x10=round(fuel_now * 10), capacity_x10=1200,
                      per_lap_x100=round(FUEL_PER_LAP * 100)),
            race=Race(gap_ahead_ms=1400 if self._position > 1 else 0x7FFFFFFF,
                      gap_behind_ms=-900, laps_left=max(0, laps - self._lap + 1), car_count=30,
                      track_limits=self._limits, track_limits_max=4,
                      pit_window=PitWindow.CLOSED, bits=race_bits),
            weather=Weather(rain, rain10, rain30, wet, grip_pct=100 - wet * 7, wind_kmh=14,
                            wind_dir_deg=250, clouds_pct=60 + rain30 * 10),
        )
