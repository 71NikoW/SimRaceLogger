import pytest

from rigdash.model import Flag, RaceFlag, Session, Telemetry
from rigdash.protocol import (
    MAX_PAYLOAD, MSG_HELLO, MSG_TELEMETRY, MSG_TRACK_INFO, TELEMETRY_SIZE,
    FrameParser, crc16, decode_telemetry, encode_frame, encode_hello,
    encode_telemetry, encode_track_info,
)
from rigdash.vectors import TELEMETRY_BASIC


def test_crc16_ccitt_false_check_value():
    assert crc16(b"123456789") == 0x29B1


def test_telemetry_payload_size():
    assert TELEMETRY_SIZE == 38
    assert len(encode_telemetry(Telemetry())) == 3 + 38 + 2


def test_telemetry_roundtrip():
    frames = list(FrameParser().feed(encode_telemetry(TELEMETRY_BASIC)))
    assert frames[0][0] == MSG_TELEMETRY
    assert decode_telemetry(frames[0][1]) == TELEMETRY_BASIC


def test_values_are_clamped_not_overflowing():
    t = Telemetry(gear=-5, position=300, track_pos=20000, fuel_laps_x10=-1)
    out = decode_telemetry(next(FrameParser().feed(encode_telemetry(t)))[1])
    assert (out.gear, out.position, out.track_pos, out.fuel_laps_x10) == (-1, 255, 10000, 0)


def test_payload_too_long_rejected():
    with pytest.raises(ValueError):
        encode_frame(0x20, bytes(MAX_PAYLOAD + 1))


def test_track_name_truncated_on_utf8_boundary():
    frame = encode_track_info("Nürburgring " + "ö" * 40)
    (_, payload), = FrameParser().feed(frame)
    assert len(payload) <= 48
    payload.decode("utf-8")  # must not raise


def test_parser_handles_byte_by_byte_and_garbage():
    stream = b"\x00\xff\xa5garbage" + encode_hello() + b"\x13\x37" + encode_track_info("Spa")
    parser = FrameParser()
    got = [f for b in stream for f in parser.feed(bytes([b]))]
    assert got == [(MSG_HELLO, b"\x01"), (MSG_TRACK_INFO, b"Spa")]


def test_parser_drops_corrupted_frame_and_recovers():
    bad = bytearray(encode_telemetry(TELEMETRY_BASIC))
    bad[10] ^= 0x01
    parser = FrameParser()
    got = list(parser.feed(bytes(bad) + encode_hello()))
    assert got == [(MSG_HELLO, b"\x01")]
    assert parser.crc_errors >= 1


def test_unknown_session_value_maps_to_unknown():
    payload = bytearray(encode_telemetry(Telemetry(session=Session.RACE))[3:-2])
    payload[36] = 99
    assert decode_telemetry(bytes(payload)).session is Session.UNKNOWN


def test_race_flag_roundtrip_and_unknown_value():
    payload = bytearray(encode_telemetry(Telemetry(race_flag=RaceFlag.BLUE))[3:-2])
    assert decode_telemetry(bytes(payload)).race_flag is RaceFlag.BLUE
    payload[37] = 8
    assert decode_telemetry(bytes(payload)).race_flag is RaceFlag.SAFETY_CAR
    payload[37] = 200
    assert decode_telemetry(bytes(payload)).race_flag is RaceFlag.NONE


def test_old_37_byte_payload_still_decodes():
    payload = encode_telemetry(Telemetry(lap_number=7, race_flag=RaceFlag.RED))[3:-2][:37]
    t = decode_telemetry(payload)
    assert t.lap_number == 7 and t.race_flag is RaceFlag.NONE


def test_reserved_flag_bits_ignored():
    t = Telemetry(flags=Flag(0))
    payload = bytearray(encode_telemetry(t)[3:-2])
    payload[34:36] = (0xFFFF).to_bytes(2, "little")
    assert decode_telemetry(bytes(payload)).flags == (
        Flag.PIT_LIMITER | Flag.IN_PIT | Flag.LAP_INVALID | Flag.ENGINE_ON | Flag.IGNITION_ON)
