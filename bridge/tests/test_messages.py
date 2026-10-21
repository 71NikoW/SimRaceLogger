"""Round trips for every message type except TELEMETRY (see test_protocol)."""

import pytest

from rigdash.model import (
    Car, Compound, Config, Corner, Engine, Fuel, LapHistory, Penalty, PitWindow, Race, RaceBits,
    SectorState, Timing, Tyres, Units, Weather,
)
from rigdash.protocol import (
    MSG_CAR, MSG_CONFIG, MSG_ENGINE, MSG_FUEL, MSG_LAP_HISTORY, MSG_RACE, MSG_TIMING, MSG_TYRES,
    MSG_WEATHER, FrameParser, decode_car, decode_config, decode_engine, decode_fuel,
    decode_lap_history, decode_race, decode_timing, decode_tyres, decode_weather, encode_car,
    encode_config, encode_engine, encode_fuel, encode_lap_history, encode_race, encode_timing,
    encode_tyres, encode_weather,
)

TYRES = Tyres([Corner(855, 2750, 97, 480), Corner(862, 2761, 96, 495), Corner(-12, 2700, 99, 20),
               Corner(810, 2705, 98, 410)], 800, 950, 2750, Compound.MEDIUM)

CASES = [
    (MSG_ENGINE, encode_engine, decode_engine, Engine(7450, 7800, 8300), 6),
    (MSG_TYRES, encode_tyres, decode_tyres, TYRES, 35),
    (MSG_CAR, encode_car, decode_car, Car(3, 4, 2, 1, 565, 12), 7),
    (MSG_FUEL, encode_fuel, decode_fuel, Fuel(425, 281, 1000, -32), 8),
    (MSG_RACE, encode_race, decode_race,
     Race(1834, -2510, 1234, 0xFFFF, 24, Penalty.DRIVE_THROUGH, 0, 2, 4, PitWindow.OPEN,
          RaceBits.MANDATORY_STOP_DONE | RaceBits.LAST_LAP), 21),
    (MSG_TIMING, encode_timing, decode_timing,
     Timing(1, [29_512, 0, 0], [29_400, 31_002, 27_870], [SectorState.SLOWER, SectorState.NONE,
            SectorState.SESSION_BEST], 88_272, 88_105), 36),
    (MSG_WEATHER, encode_weather, decode_weather, Weather(2, 3, 4, 1, 92, 12, 225, 80), 9),
    (MSG_LAP_HISTORY, encode_lap_history, decode_lap_history,
     LapHistory([(88_201, True), (89_012, False), (87_950, True)]), 22),
    (MSG_CONFIG, encode_config, decode_config, Config(0b1010_0101, 128, Units.MPH | Units.BAR), 4),
]


@pytest.mark.parametrize("msg_type,enc,dec,value,size", CASES, ids=[c[0] for c in CASES])
def test_roundtrip(msg_type, enc, dec, value, size):
    (got_type, payload), = FrameParser().feed(enc(value))
    assert got_type == msg_type
    assert len(payload) == size
    assert dec(payload) == value


def test_clamping():
    w = decode_weather(next(FrameParser().feed(encode_weather(Weather(rain=9, grip_pct=150,
                                                                      wind_dir_deg=725))))[1])
    assert (w.rain, w.grip_pct, w.wind_dir_deg) == (5, 100, 5)
    c = decode_car(next(FrameParser().feed(encode_car(Car(abs=-3, damage_pct=300))))[1])
    assert (c.abs, c.damage_pct) == (0, 100)


def test_history_keeps_newest_five():
    laps = [(80_000 + i, i % 2 == 0) for i in range(8)]
    h = decode_lap_history(next(FrameParser().feed(encode_lap_history(LapHistory(laps))))[1])
    assert h.laps == laps[:5]


def test_tyres_need_four_wheels():
    with pytest.raises(ValueError):
        encode_tyres(Tyres([Corner()] * 3))


def test_unknown_enum_values_fall_back():
    payload = bytearray(next(FrameParser().feed(encode_race(Race())))[1])
    payload[15] = 99  # penalty
    payload[19] = 99  # pit window
    r = decode_race(bytes(payload))
    assert r.penalty is Penalty.NONE and r.pit_window is PitWindow.UNKNOWN
