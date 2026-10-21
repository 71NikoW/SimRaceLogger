import json
import re
from pathlib import Path

from rigdash.derived import Enricher
from rigdash.model import Flag, RaceBits, RaceFlag
from rigdash.sources.spa import (
    COOL_DOWN_FRAC, SpaSource, gear_for, load_track, rpm_for,
)

ROOT = Path(__file__).resolve().parents[2]


def run(src, until_ms, step=250):
    return [(t, src.poll(t)) for t in range(0, until_ms, step)]


def test_lap_time_and_sectors_are_realistic():
    src = SpaSource()
    assert src.normal.lap_ms == 138_400                  # GT3-ish 2:18.4
    s1, s2 = (src.normal.time_at(f * 7004) / 1000 for f in src.track["sectors"])
    assert 30 < s1 < 45 and 60 < s2 - s1 < 75             # Spa's long middle sector
    assert src.sc.lap_ms > src.normal.lap_ms * 1.15        # safety car lap is slower


def test_speeds_gears_and_rpm():
    src = SpaSource()
    speeds = [src.normal.speed(d) for d in range(0, 7004, 10)]
    assert 260 < max(speeds) < 300 and 60 < min(speeds) < 90
    assert gear_for(70) == 1 and gear_for(275) == 6
    for v in (80, 150, 230, 280):
        assert 3000 <= rpm_for(v, gear_for(v)) <= 7800 + 1  # never past the shift point
    names = [src.corner_at(d) for d in (0, 340, 950, 3450, 6700)]
    assert names == ["START / FINISH", "LA SOURCE", "EAU ROUGE", "POUHON", "BUS STOP"]


def test_script_flags_in_order():
    src = SpaSource()
    seen = []
    for _, snap in run(src, 900_000, 500):
        t = snap.telemetry
        key = (t.lap_number, t.race_flag)
        if not seen or seen[-1] != key:
            seen.append(key)
    flags = [f for _, f in seen]
    # pit exit green, yellow at Pouhon, blue at Blanchimont, SC lap, green restart, chequered
    order = [RaceFlag.YELLOW, RaceFlag.BLUE, RaceFlag.SAFETY_CAR, RaceFlag.GREEN, RaceFlag.YELLOW,
             RaceFlag.CHEQUERED]
    it = iter(flags)
    assert all(f in it for f in order), flags
    assert (4, RaceFlag.SAFETY_CAR) in seen and (7, RaceFlag.CHEQUERED) in seen


def test_pit_exit_invalid_lap_weather_and_last_lap():
    snaps = [s for _, s in run(SpaSource(), 880_000, 500)]
    assert snaps[0].telemetry.flags & Flag.PIT_LIMITER
    lap2 = [s for s in snaps if s.telemetry.lap_number == 2]
    assert any(s.telemetry.flags & Flag.LAP_INVALID for s in lap2)
    assert any(s.weather.rain_in_10 >= 2 and s.weather.rain == 0 for s in snaps)
    assert any(s.weather.rain >= 2 for s in snaps)
    assert any(s.race.bits & RaceBits.LAST_LAP for s in snaps if s.telemetry.lap_number == 6)
    # invalid lap 2 never becomes the best lap
    lap4 = next(s for s in snaps if s.telemetry.lap_number == 4).telemetry
    assert lap4.best_lap_ms == 138_400 and lap4.last_lap_ms == 138_400


def test_restarts_after_chequered_flag():
    src = SpaSource()
    total = 5 * 138_400 + src.sc.lap_ms + int(COOL_DOWN_FRAC * 138_400) + 5000
    last = src.poll(total)
    assert last.telemetry.lap_number == 1
    assert last.telemetry.flags & Flag.PIT_LIMITER


def test_enricher_builds_sector_colours_on_spa():
    src, e = SpaSource(), Enricher()
    snap = None
    for _, snap in run(src, 3 * 138_400 + 50_000, 100):
        e.apply(snap)
    tm = snap.timing
    assert all(tm.best_sector_ms) and abs(tm.optimal_lap_ms - 138_400) < 300


def test_simulator_embeds_the_same_track():
    html = (ROOT / "simulator" / "index.html").read_text()
    m = re.search(r'<script type="application/json" id="spa-track">(.*?)</script>', html, re.S)
    assert m, "simulator must embed tracks/spa.json"
    assert json.loads(m.group(1)) == load_track("spa")
