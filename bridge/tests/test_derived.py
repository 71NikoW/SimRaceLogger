from rigdash.derived import Enricher, FuelCalculator, LapHistoryTracker, SectorTracker
from rigdash.model import (
    FUEL_TO_FINISH_UNKNOWN, Flag, Fuel, Race, SectorState, Snapshot, Telemetry,
)


def test_fuel_average_ignores_first_partial_lap_and_pit_laps():
    f = FuelCalculator()
    f.update(1, 50.0)     # joined mid-lap
    f.update(2, 48.0)     # partial lap, ignored
    assert f.per_lap is None
    f.update(3, 45.0)     # 3.0 used
    f.update(3, 44.0, in_pit=True)
    f.update(4, 80.0)     # refuelled lap, ignored
    f.update(5, 77.2)     # 2.8
    assert abs(f.per_lap - 2.9) < 1e-9
    assert abs(f.laps_remaining(29.0) - 10.0) < 1e-9


def test_fuel_refuel_while_rolling_is_ignored():
    f = FuelCalculator()
    f.update(1, 50.0)
    f.update(2, 47.0)
    f.update(2, 60.0)     # fuel went up mid-lap (e.g. reset)
    f.update(3, 57.0)
    assert f.per_lap is None


def test_fuel_to_finish_laps_and_time():
    f = FuelCalculator()
    for lap, fuel in enumerate([60.0, 57.0, 54.0, 51.0], start=1):
        f.update(lap, fuel)
    assert f.per_lap == 3.0
    assert f.to_finish(30.0, laps_left=10) == (10.5 * 3.0) - 30.0
    # 20 min left, 90 s laps -> ceil(13.33) + 1 = 15 laps
    assert f.to_finish(30.0, time_left_s=1200, lap_ms=90_000) == (15.5 * 3.0) - 30.0
    assert f.to_finish(30.0) is None
    assert FuelCalculator.encode_to_finish(None) == FUEL_TO_FINISH_UNKNOWN
    assert FuelCalculator.encode_to_finish(1.55) == 16


def drive_lap(st, lap, lap_ms, sectors, invalid=False, start_ms=0):
    """Feed one lap with given sector times (ms)."""
    t = 0
    bounds = [0, 3334, 6667, 10000]  # first sample of each sector
    for s, dur in enumerate(sectors):
        for k in range(10):
            pos = bounds[s] + (bounds[s + 1] - bounds[s]) * k // 10
            st.update(lap, pos, t + dur * k // 10, invalid)
        t += dur
    st.update(lap + 1, 10, 50, False, last_lap_ms=sum(sectors))


def test_sector_colours():
    st = SectorTracker()
    st.update(1, 9000, 80_000, False)  # join late in lap 1
    st.update(2, 0, 0, False, last_lap_ms=0)
    drive_lap(st, 2, 90_000, [30_000, 31_000, 29_000])
    tm = st.timing()
    assert tm.best_sector_ms == [30_000, 31_000, 29_000]
    assert tm.optimal_lap_ms == 90_000 and tm.session_best_ms == 90_000
    drive_lap(st, 3, 0, [29_500, 31_500, 28_800])
    tm = st.timing()
    # S2/S3 colours of lap 3 are kept until lap 4 reaches them.
    assert tm.state[1] == SectorState.SLOWER and tm.state[2] == SectorState.SESSION_BEST
    assert tm.best_sector_ms == [29_500, 31_000, 28_800]
    assert tm.optimal_lap_ms == 29_500 + 31_000 + 28_800


def test_sector_personal_best_vs_session_best():
    st = SectorTracker()
    st.update(1, 9999, 1, False)
    st.update(2, 0, 0, False)
    st.note_session_best(0, 28_000)  # someone else is faster
    for k in range(4):
        st.update(2, 1000 * k, 3000 * k, False)
    st.update(2, 3400, 30_000, False)  # cross into S2 after 30.0 s
    assert st.timing().state[0] == SectorState.PERSONAL_BEST
    assert st.timing().sector_ms[0] == 30_000


def test_invalid_lap_sectors_never_best():
    st = SectorTracker()
    st.update(1, 9999, 1, False)
    st.update(2, 0, 0, False)
    drive_lap(st, 2, 0, [30_000, 31_000, 29_000], invalid=True)
    assert st.timing().best_sector_ms == [0, 0, 0]
    assert st.timing().optimal_lap_ms == 0


def test_lap_history():
    h = LapHistoryTracker()
    assert not h.update(1, 0, False)
    h.update(1, 0, True)               # lap 1 invalid
    assert h.update(2, 89_000, False)
    h.update(2, 89_000, False)
    assert h.update(3, 88_000, False)
    assert h.history().laps == [(88_000, True), (89_000, False)]
    for lap in range(4, 10):
        h.update(lap, 80_000 + lap, False)
    assert len(h.history().laps) == 5


def test_enricher_fills_fuel_laps_and_to_finish():
    e = Enricher()
    snap = None
    for lap, fuel in enumerate([600, 570, 540, 510], start=1):
        t = Telemetry(lap_number=lap, last_lap_ms=90_000 if lap > 1 else 0, best_lap_ms=90_000)
        snap = e.apply(Snapshot(t, fuel=Fuel(liters_x10=fuel), race=Race(laps_left=20)))
    assert snap.fuel.per_lap_x100 == 300
    assert snap.telemetry.fuel_laps_x10 == 170
    assert snap.fuel.to_finish_x10 == round((20.5 * 3 - 51) * 10)
    assert snap.timing is not None and snap.history is not None


def test_enricher_keeps_source_values():
    e = Enricher()
    t = Telemetry(lap_number=1, fuel_laps_x10=55, flags=Flag.ENGINE_ON)
    snap = e.apply(Snapshot(t, fuel=Fuel(liters_x10=100, per_lap_x100=250)))
    assert snap.fuel.per_lap_x100 == 250 and snap.telemetry.fuel_laps_x10 == 55
