"""Serial protocol v1 — see docs/PROTOCOL.md. Mirrors firmware/lib/core."""

import struct
from typing import Iterator

from .model import (
    Car, Compound, Config, Corner, Engine, Flag, Fuel, LapHistory, Penalty, PitWindow, Race,
    RaceBits, RaceFlag, SectorState, Session, Telemetry, Timing, Tyres, Units, Weather,
)

PROTOCOL_VERSION = 1
SOF = 0xA5
MAX_PAYLOAD = 64
MAX_TRACK_NAME = 48

MSG_HELLO = 0x01
MSG_TELEMETRY = 0x10
MSG_TRACK_INFO = 0x11
MSG_ENGINE = 0x12
MSG_TYRES = 0x13
MSG_CAR = 0x14
MSG_FUEL = 0x15
MSG_RACE = 0x16
MSG_TIMING = 0x17
MSG_WEATHER = 0x18
MSG_LAP_HISTORY = 0x19
MSG_CONFIG = 0x20

_TELEMETRY = struct.Struct("<IIIIiHBbHHHhhHBB")
TELEMETRY_SIZE = _TELEMETRY.size  # 38
# v1.0 payloads ended before race_flag; still accepted.
TELEMETRY_MIN_SIZE = 37


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def encode_frame(msg_type: int, payload: bytes) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too long: {len(payload)} > {MAX_PAYLOAD}")
    body = bytes([msg_type, len(payload)]) + payload
    return bytes([SOF]) + body + struct.pack("<H", crc16(body))


def encode_hello() -> bytes:
    return encode_frame(MSG_HELLO, bytes([PROTOCOL_VERSION]))


def _clamp(value: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, int(value)))


def encode_telemetry(t: Telemetry) -> bytes:
    payload = _TELEMETRY.pack(
        _clamp(t.current_lap_ms, 0, 0xFFFFFFFF),
        _clamp(t.last_lap_ms, 0, 0xFFFFFFFF),
        _clamp(t.best_lap_ms, 0, 0xFFFFFFFF),
        _clamp(t.last_pit_ms, 0, 0xFFFFFFFF),
        _clamp(t.delta_ms, -0x80000000, 0x7FFFFFFF),
        _clamp(t.lap_number, 0, 0xFFFF),
        _clamp(t.position, 0, 0xFF),
        _clamp(t.gear, -1, 127),
        _clamp(t.speed_kmh, 0, 0xFFFF),
        _clamp(t.fuel_laps_x10, 0, 0xFFFF),
        _clamp(t.track_pos, 0, 10000),
        _clamp(t.air_temp_x10, -0x8000, 0x7FFF),
        _clamp(t.track_temp_x10, -0x8000, 0x7FFF),
        int(t.flags) & 0xFFFF,
        int(t.session) & 0xFF,
        int(t.race_flag) & 0xFF,
    )
    return encode_frame(MSG_TELEMETRY, payload)


def encode_track_info(name: str) -> bytes:
    raw = name.encode("utf-8")[:MAX_TRACK_NAME]
    # Don't cut a multi-byte UTF-8 sequence in half.
    raw = raw.decode("utf-8", errors="ignore").encode("utf-8")
    return encode_frame(MSG_TRACK_INFO, raw)


def decode_telemetry(payload: bytes) -> Telemetry:
    if len(payload) < TELEMETRY_MIN_SIZE:
        raise ValueError("telemetry payload too short")
    v = _TELEMETRY.unpack_from(bytes(payload[:TELEMETRY_SIZE]).ljust(TELEMETRY_SIZE, b"\0"))
    return Telemetry(
        current_lap_ms=v[0], last_lap_ms=v[1], best_lap_ms=v[2], last_pit_ms=v[3],
        delta_ms=v[4], lap_number=v[5], position=v[6], gear=v[7], speed_kmh=v[8],
        fuel_laps_x10=v[9], track_pos=v[10], air_temp_x10=v[11], track_temp_x10=v[12],
        flags=Flag(v[13] & 0x1F), session=Session(v[14]) if v[14] <= 4 else Session.UNKNOWN,
        race_flag=RaceFlag(v[15]) if v[15] <= 9 else RaceFlag.NONE,
    )


_ENGINE = struct.Struct("<HHH")                 # 6
_CORNER = struct.Struct("<hHBh")                # 7, x4
_TYRES_TAIL = struct.Struct("<hhHB")            # 7  -> 35
_CAR = struct.Struct("<BBBBHB")                 # 7
_FUEL = struct.Struct("<HHHh")                  # 8
_RACE = struct.Struct("<iiIHBBBBBBB")           # 21
_TIMING = struct.Struct("<B3I3I3BII")           # 36
_WEATHER = struct.Struct("<BBBBBBHB")           # 9
_HISTORY = struct.Struct("<B5IB")               # 22
_CONFIG = struct.Struct("<HBB")                 # 4


def _u8(v: int) -> int:
    return _clamp(v, 0, 0xFF)


def _u16(v: int) -> int:
    return _clamp(v, 0, 0xFFFF)


def _i16(v: int) -> int:
    return _clamp(v, -0x8000, 0x7FFF)


def _u32(v: int) -> int:
    return _clamp(v, 0, 0xFFFFFFFF)


def _i32(v: int) -> int:
    return _clamp(v, -0x80000000, 0x7FFFFFFF)


def _level(v: int) -> int:
    return _clamp(v, 0, 5)


def encode_engine(e: Engine) -> bytes:
    return encode_frame(MSG_ENGINE, _ENGINE.pack(_u16(e.rpm), _u16(e.shift_rpm), _u16(e.max_rpm)))


def encode_tyres(t: Tyres) -> bytes:
    if len(t.wheels) != 4:
        raise ValueError("tyres need exactly 4 wheels (FL, FR, RL, RR)")
    payload = b"".join(
        _CORNER.pack(_i16(w.temp_x10), _u16(w.pressure_psi_x100), _clamp(w.wear_pct, 0, 100),
                     _i16(w.brake_temp_c)) for w in t.wheels)
    payload += _TYRES_TAIL.pack(_i16(t.opt_min_x10), _i16(t.opt_max_x10), _u16(t.target_psi_x100),
                                int(t.compound))
    return encode_frame(MSG_TYRES, payload)


def encode_car(c: Car) -> bytes:
    return encode_frame(MSG_CAR, _CAR.pack(_u8(c.abs), _u8(c.tc), _u8(c.tc_cut), _u8(c.engine_map),
                                           _u16(c.brake_bias_x10), _clamp(c.damage_pct, 0, 100)))


def encode_fuel(f: Fuel) -> bytes:
    return encode_frame(MSG_FUEL, _FUEL.pack(_u16(f.liters_x10), _u16(f.per_lap_x100),
                                             _u16(f.capacity_x10), _i16(f.to_finish_x10)))


def encode_race(r: Race) -> bytes:
    return encode_frame(MSG_RACE, _RACE.pack(
        _i32(r.gap_ahead_ms), _i32(r.gap_behind_ms), _u32(r.time_left_s), _u16(r.laps_left),
        _u8(r.car_count), int(r.penalty), _u8(r.penalty_s), _u8(r.track_limits),
        _u8(r.track_limits_max), int(r.pit_window), int(r.bits) & 0x03))


def encode_timing(t: Timing) -> bytes:
    return encode_frame(MSG_TIMING, _TIMING.pack(
        _clamp(t.sector, 0, 2), *(_u32(v) for v in t.sector_ms), *(_u32(v) for v in t.best_sector_ms),
        *(int(v) for v in t.state), _u32(t.optimal_lap_ms), _u32(t.session_best_ms)))


def encode_weather(w: Weather) -> bytes:
    return encode_frame(MSG_WEATHER, _WEATHER.pack(
        _level(w.rain), _level(w.rain_in_10), _level(w.rain_in_30), _level(w.wetness),
        _clamp(w.grip_pct, 0, 100), _u8(w.wind_kmh), int(w.wind_dir_deg) % 360,
        _clamp(w.clouds_pct, 0, 100)))


def encode_lap_history(h: LapHistory) -> bytes:
    laps = h.laps[:5]
    times = [_u32(ms) for ms, _ in laps] + [0] * (5 - len(laps))
    mask = sum(1 << i for i, (_, valid) in enumerate(laps) if valid)
    return encode_frame(MSG_LAP_HISTORY, _HISTORY.pack(len(laps), *times, mask))


def encode_config(c: Config) -> bytes:
    return encode_frame(MSG_CONFIG, _CONFIG.pack(_u16(c.page_mask), _u8(c.brightness), int(c.units) & 0x07))


def _enum(cls, v, default):
    try:
        return cls(v)
    except ValueError:
        return default


def decode_engine(p: bytes) -> Engine:
    return Engine(*_ENGINE.unpack_from(p))


def decode_tyres(p: bytes) -> Tyres:
    wheels = [Corner(*_CORNER.unpack_from(p, i * 7)) for i in range(4)]
    lo, hi, psi, comp = _TYRES_TAIL.unpack_from(p, 28)
    return Tyres(wheels, lo, hi, psi, _enum(Compound, comp, Compound.UNKNOWN))


def decode_car(p: bytes) -> Car:
    return Car(*_CAR.unpack_from(p))


def decode_fuel(p: bytes) -> Fuel:
    return Fuel(*_FUEL.unpack_from(p))


def decode_race(p: bytes) -> Race:
    v = _RACE.unpack_from(p)
    return Race(v[0], v[1], v[2], v[3], v[4], _enum(Penalty, v[5], Penalty.NONE), v[6], v[7], v[8],
                _enum(PitWindow, v[9], PitWindow.UNKNOWN), RaceBits(v[10] & 0x03))


def decode_timing(p: bytes) -> Timing:
    v = _TIMING.unpack_from(p)
    return Timing(v[0], list(v[1:4]), list(v[4:7]),
                  [_enum(SectorState, x, SectorState.NONE) for x in v[7:10]], v[10], v[11])


def decode_weather(p: bytes) -> Weather:
    return Weather(*_WEATHER.unpack_from(p))


def decode_lap_history(p: bytes) -> LapHistory:
    v = _HISTORY.unpack_from(p)
    n = min(v[0], 5)
    return LapHistory([(v[1 + i], bool(v[6] >> i & 1)) for i in range(n)])


def decode_config(p: bytes) -> Config:
    mask, bright, units = _CONFIG.unpack_from(p)
    return Config(mask, bright, Units(units & 0x07))


class FrameParser:
    """Streaming parser, same state machine as the firmware."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self.crc_errors = 0

    def feed(self, data: bytes) -> Iterator[tuple[int, bytes]]:
        self._buf.extend(data)
        while True:
            start = self._buf.find(SOF)
            if start < 0:
                self._buf.clear()
                return
            del self._buf[:start]
            if len(self._buf) < 3:
                return
            length = self._buf[2]
            if length > MAX_PAYLOAD:
                del self._buf[0]
                continue
            total = 3 + length + 2
            if len(self._buf) < total:
                return
            body = bytes(self._buf[1:3 + length])
            (crc,) = struct.unpack_from("<H", self._buf, 3 + length)
            if crc != crc16(body):
                self.crc_errors += 1
                del self._buf[0]
                continue
            del self._buf[:total]
            yield body[0], body[2:]
