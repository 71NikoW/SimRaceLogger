from rigdash.model import Flag
from rigdash.sources import DemoSource
from rigdash.sources.demo import weather_at


def run(src, until_ms, step=100):
    return [src.poll(t).telemetry for t in range(0, until_ms, step)]


def test_starts_in_pit_with_limiter():
    t = DemoSource().poll(0).telemetry
    assert t.flags & Flag.PIT_LIMITER and t.flags & Flag.IN_PIT
    assert t.lap_number == 1 and t.current_lap_ms == 0


def test_limiter_off_after_pit_exit():
    t = run(DemoSource(), DemoSource.PIT_EXIT_MS + 200)[-1]
    assert not t.flags & Flag.PIT_LIMITER


def test_laps_advance_and_fuel_decreases_while_driving():
    src = DemoSource(lap_ms=10_000, fuel_laps=5.0, invalid_chance=0.0)
    frames = run(src, 35_000)
    last = frames[-1]
    assert last.lap_number == 4
    assert 8_500 <= last.last_lap_ms <= 11_500
    assert last.best_lap_ms == min(f.last_lap_ms for f in frames if f.last_lap_ms)
    # No fuel used in the pit lane (first 15 s), ~2 laps of driving after that.
    assert 28 <= last.fuel_laps_x10 <= 32


def test_current_lap_time_monotonic_within_lap():
    times = [f.current_lap_ms for f in run(DemoSource(lap_ms=10_000, invalid_chance=0.0), 9_000)]
    assert times == sorted(times)


def test_invalid_laps_never_become_best():
    assert run(DemoSource(lap_ms=5_000, invalid_chance=1.0), 30_000)[-1].best_lap_ms == 0


def test_deterministic_with_seed():
    a = [DemoSource(seed=7).poll(t) for t in range(0, 20_000, 500)]
    b = [DemoSource(seed=7).poll(t) for t in range(0, 20_000, 500)]
    assert a[0] == b[0]


def test_all_parts_present_and_plausible():
    src = DemoSource()
    for t in range(0, 300_000, 1000):
        snap = src.poll(t)
    assert snap.engine.shift_rpm > 0 and 0 < snap.engine.rpm <= snap.engine.max_rpm
    assert len(snap.tyres.wheels) == 4
    assert all(700 <= w.temp_x10 <= 1000 for w in snap.tyres.wheels)  # warmed up after 5 min
    assert snap.car.brake_bias_x10 == 565
    assert snap.race.time_left_s == 30 * 60 - 299


def test_weather_cycle_has_forecast_before_rain():
    assert weather_at(0) == (0, 0, 0, 0)
    rain, rain10, _, _ = weather_at(5 * 60)
    assert rain == 0 and rain10 >= 2      # rain forecast first …
    assert weather_at(7 * 60)[0] >= 2     # … then it rains
    assert weather_at(13 * 60)[0] == 0 and weather_at(13 * 60)[3] > 0  # drying
