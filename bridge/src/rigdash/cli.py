"""rigdash-bridge: read a telemetry source and stream frames to the display."""

import argparse
import sys
import time

from .protocol import encode_hello, encode_telemetry, encode_track_info
from .sources import SOURCES

HELLO_INTERVAL_MS = 2000


def _now_ms() -> int:
    return time.monotonic_ns() // 1_000_000


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="rigdash-bridge")
    p.add_argument("--source", choices=sorted(SOURCES), default="demo")
    p.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--rate", type=float, default=30.0, help="frames per second")
    p.add_argument("--dry-run", action="store_true", help="print frames as hex instead of sending")
    p.add_argument("--frames", type=int, default=0, help="stop after N telemetry frames (0 = forever)")
    args = p.parse_args(argv)

    if not args.dry_run and not args.port:
        p.error("--port is required unless --dry-run is given")

    if args.dry_run:
        def write(frame: bytes) -> None:
            print(frame.hex(" ").upper())
    else:
        try:
            import serial  # type: ignore
        except ImportError:
            print("pyserial missing: pip install 'rigdash[serial]'", file=sys.stderr)
            return 2
        port = serial.Serial(args.port, args.baud, timeout=0)
        write = port.write

    source = SOURCES[args.source]()
    period = 1.0 / args.rate
    sent = 0
    last_hello = -HELLO_INTERVAL_MS
    try:
        while args.frames == 0 or sent < args.frames:
            now = _now_ms()
            if now - last_hello >= HELLO_INTERVAL_MS:
                write(encode_hello())
                if source.track_name():
                    write(encode_track_info(source.track_name()))
                last_hello = now
            t = source.poll(now)
            if t is not None:
                write(encode_telemetry(t))
                sent += 1
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    finally:
        source.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
