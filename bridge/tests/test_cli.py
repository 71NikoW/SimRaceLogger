import argparse

import pytest

from rigdash.cli import main, parse_pages, parse_units
from rigdash.model import Units
from rigdash.protocol import (
    MSG_CONFIG, MSG_ENGINE, MSG_HELLO, MSG_TELEMETRY, MSG_TRACK_INFO, MSG_TYRES, FrameParser,
    decode_config,
)


def test_dry_run_emits_valid_frames(capsys):
    assert main(["--dry-run", "--frames", "3", "--rate", "1000", "--pages", "current,tyres",
                 "--units", "mph,bar"]) == 0
    raw = b"".join(bytes.fromhex(line) for line in capsys.readouterr().out.splitlines())
    frames = list(FrameParser().feed(raw))
    types = [t for t, _ in frames]
    assert types[:3] == [MSG_HELLO, MSG_CONFIG, MSG_TRACK_INFO]
    assert types.count(MSG_TELEMETRY) == 3
    assert types.count(MSG_ENGINE) == 3
    assert MSG_TYRES in types
    cfg = decode_config(frames[1][1])
    assert cfg.page_mask == 0b101 and cfg.units == Units.MPH | Units.BAR


def test_log_option_writes_csv(tmp_path, capsys):
    log = tmp_path / "laps.csv"
    assert main(["--dry-run", "--frames", "2", "--rate", "1000", "--log", str(log)]) == 0
    assert log.read_text().startswith("timestamp,track,lap")


def test_parse_pages_and_units():
    assert parse_pages("all") == 0xFF
    assert parse_pages("current, laps") == 0b1000_0001
    with pytest.raises(argparse.ArgumentTypeError):
        parse_pages("nope")
    assert parse_units("metric") == Units.METRIC
    assert parse_units("fahrenheit") == Units.FAHRENHEIT
    with pytest.raises(argparse.ArgumentTypeError):
        parse_units("kelvin")


def test_port_required_without_dry_run():
    with pytest.raises(SystemExit):
        main([])
