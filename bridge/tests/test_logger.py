import csv

from rigdash.logger import FIELDS, SessionLogger, fmt_lap
from rigdash.model import Flag, Fuel, Snapshot, Telemetry, Timing, Weather


def snap(lap, last=0, fuel=500, invalid=False, s=(0, 0)):
    t = Telemetry(lap_number=lap, last_lap_ms=last, position=4, air_temp_x10=255,
                  flags=Flag.LAP_INVALID if invalid else Flag.NONE)
    return Snapshot(t, fuel=Fuel(liters_x10=fuel), weather=Weather(rain=2),
                    timing=Timing(sector_ms=[s[0], s[1], 0]))


def test_one_row_per_lap(tmp_path):
    path = tmp_path / "s.csv"
    log = SessionLogger(path, "Spa", clock=lambda: "T")
    assert not log.update(snap(1, fuel=500))
    log.update(snap(1, fuel=480, s=(30_000, 0)))
    log.update(snap(1, fuel=470, invalid=True, s=(30_000, 31_000)))
    assert log.update(snap(2, last=90_123, fuel=470))
    assert not log.update(snap(2, last=90_123, fuel=468))
    assert log.update(snap(3, last=88_000, fuel=440))
    log.close()
    rows = list(csv.DictReader(path.open()))
    assert [r["lap"] for r in rows] == ["1", "2"]
    first = rows[0]
    assert first["lap_time"] == "1:30.123" and first["valid"] == "0"
    assert (first["s1_ms"], first["s2_ms"], first["s3_ms"]) == ("30000", "31000", "29123")
    assert first["fuel_used_l"] == "3.00" and first["track"] == "Spa"
    assert rows[1]["valid"] == "1" and rows[1]["s1_ms"] == ""


def test_appends_without_second_header(tmp_path):
    path = tmp_path / "s.csv"
    for _ in range(2):
        log = SessionLogger(path)
        log.update(snap(1))
        log.update(snap(2, last=90_000))
        log.close()
    lines = path.read_text().splitlines()
    assert lines[0] == ",".join(FIELDS)
    assert len(lines) == 3


def test_fmt_lap():
    assert fmt_lap(0) == ""
    assert fmt_lap(61_005) == "1:01.005"
