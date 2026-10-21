from types import SimpleNamespace as P

from rigdash.model import Config, Snapshot, Telemetry, Timing, Weather
from rigdash.ports import find_display_port
from rigdash.protocol import (
    MSG_CONFIG, MSG_HELLO, MSG_TELEMETRY, MSG_TIMING, MSG_TRACK_INFO, MSG_WEATHER, FrameParser,
)
from rigdash.scheduler import FrameScheduler


def test_port_prefers_espressif_native_usb():
    ports = [P(device="COM3", vid=0x1A86), P(device="COM9", vid=0x303A), P(device="COM1", vid=None)]
    assert find_display_port(ports) == "COM9"
    assert find_display_port(ports[:1] + ports[2:]) == "COM3"
    assert find_display_port([P(device="COM1", vid=0x8087)]) is None


def types(frames):
    return [t for f in frames for t, _ in FrameParser().feed(f)]


def test_scheduler_rates_and_on_change():
    s = FrameScheduler(Config(), "Spa")
    snap = Snapshot(Telemetry(), weather=Weather(), timing=Timing())
    first = types(s.frames(snap, 0))
    assert first[:3] == [MSG_HELLO, MSG_CONFIG, MSG_TRACK_INFO]
    assert MSG_WEATHER in first and MSG_TIMING in first
    nxt = types(s.frames(snap, 33))
    assert nxt == [MSG_TELEMETRY]  # nothing else due yet
    snap.timing = Timing(sector=1, sector_ms=[30_000, 0, 0])
    assert MSG_TIMING in types(s.frames(snap, 66))  # changed -> sent at once
    assert MSG_WEATHER in types(s.frames(snap, 1000))
    assert MSG_HELLO in types(s.frames(snap, 2000))


def test_scheduler_without_snapshot_still_says_hello():
    s = FrameScheduler()
    assert types(s.frames(None, 0)) == [MSG_HELLO, MSG_CONFIG]
    assert s.frames(None, 10) == []
