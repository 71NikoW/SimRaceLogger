from rigdash.cli import main
from rigdash.protocol import MSG_HELLO, MSG_TELEMETRY, MSG_TRACK_INFO, FrameParser


def test_dry_run_emits_valid_frames(capsys):
    assert main(["--dry-run", "--frames", "3", "--rate", "1000"]) == 0
    raw = b"".join(bytes.fromhex(line) for line in capsys.readouterr().out.splitlines())
    types = [t for t, _ in FrameParser().feed(raw)]
    assert types[:2] == [MSG_HELLO, MSG_TRACK_INFO]
    assert types.count(MSG_TELEMETRY) == 3
