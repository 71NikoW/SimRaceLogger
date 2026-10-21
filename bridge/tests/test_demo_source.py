from rigdash.model import Flag
from rigdash.sources import DemoSource


def run(src, until_ms, step=100):
    return [src.poll(t) for t in range(0, until_ms, step)]


def test_starts_in_pit_with_limiter():
    t = DemoSource().poll(0)
    assert t.flags & Flag.PIT_LIMITER and t.flags & Flag.IN_PIT
    assert t.lap_number == 1 and t.current_lap_ms == 0


def test_limiter_off_after_pit_exit():
    src = DemoSource()
    t = run(src, DemoSource.PIT_EXIT_MS + 200)[-1]
    assert not t.flags & Flag.PIT_LIMITER


def test_laps_advance_and_fuel_decreases():
    src = DemoSource(lap_ms=10_000, fuel_laps=5.0, invalid_chance=0.0)
    frames = run(src, 35_000)
    last = frames[-1]
    assert last.lap_number == 4
    assert 8_500 <= last.last_lap_ms <= 11_500
    assert last.best_lap_ms == min(f.last_lap_ms for f in frames if f.last_lap_ms)
    assert last.fuel_laps_x10 == 20


def test_current_lap_time_monotonic_within_lap():
    frames = run(DemoSource(lap_ms=10_000, invalid_chance=0.0), 9_000)
    times = [f.current_lap_ms for f in frames]
    assert times == sorted(times)


def test_invalid_laps_never_become_best():
    src = DemoSource(lap_ms=5_000, invalid_chance=1.0)
    assert run(src, 30_000)[-1].best_lap_ms == 0


def test_deterministic_with_seed():
    a = run(DemoSource(seed=7), 20_000, 500)
    b = run(DemoSource(seed=7), 20_000, 500)
    assert a == b
