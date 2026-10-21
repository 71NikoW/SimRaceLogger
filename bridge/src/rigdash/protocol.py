"""Serial protocol v1 — see docs/PROTOCOL.md. Mirrors firmware/lib/core."""

import struct
from typing import Iterator

from .model import Flag, Session, Telemetry

PROTOCOL_VERSION = 1
SOF = 0xA5
MAX_PAYLOAD = 64
MAX_TRACK_NAME = 48

MSG_HELLO = 0x01
MSG_TELEMETRY = 0x10
MSG_TRACK_INFO = 0x11

_TELEMETRY = struct.Struct("<IIIIiHBbHHHhhHB")
TELEMETRY_SIZE = _TELEMETRY.size  # 37


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
    )
    return encode_frame(MSG_TELEMETRY, payload)


def encode_track_info(name: str) -> bytes:
    raw = name.encode("utf-8")[:MAX_TRACK_NAME]
    # Don't cut a multi-byte UTF-8 sequence in half.
    raw = raw.decode("utf-8", errors="ignore").encode("utf-8")
    return encode_frame(MSG_TRACK_INFO, raw)


def decode_telemetry(payload: bytes) -> Telemetry:
    if len(payload) < TELEMETRY_SIZE:
        raise ValueError("telemetry payload too short")
    v = _TELEMETRY.unpack_from(payload)
    return Telemetry(
        current_lap_ms=v[0], last_lap_ms=v[1], best_lap_ms=v[2], last_pit_ms=v[3],
        delta_ms=v[4], lap_number=v[5], position=v[6], gear=v[7], speed_kmh=v[8],
        fuel_laps_x10=v[9], track_pos=v[10], air_temp_x10=v[11], track_temp_x10=v[12],
        flags=Flag(v[13] & 0x1F), session=Session(v[14]) if v[14] <= 4 else Session.UNKNOWN,
    )


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
