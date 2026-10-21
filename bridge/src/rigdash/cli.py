"""rigdash-bridge: read a telemetry source and stream frames to the display."""

import argparse
import sys
import time

from .derived import Enricher
from .model import Config, Units
from .scheduler import FrameScheduler
from .sources import SOURCES

PAGES = ["current", "delta", "tyres", "car", "race", "weather", "track", "laps"]
UNITS = {"mph": Units.MPH, "fahrenheit": Units.FAHRENHEIT, "bar": Units.BAR}


def _now_ms() -> int:
    return time.monotonic_ns() // 1_000_000


def parse_pages(value: str) -> int:
    if value == "all":
        return (1 << len(PAGES)) - 1
    mask = 0
    for name in value.split(","):
        name = name.strip()
        if name not in PAGES:
            raise argparse.ArgumentTypeError(f"unknown page '{name}', choose from {', '.join(PAGES)}")
        mask |= 1 << PAGES.index(name)
    return mask


def parse_units(value: str) -> Units:
    units = Units.METRIC
    for name in filter(None, (v.strip() for v in value.split(","))):
        if name == "metric":
            continue
        if name not in UNITS:
            raise argparse.ArgumentTypeError(f"unknown unit '{name}', choose from metric, {', '.join(UNITS)}")
        units |= UNITS[name]
    return units


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="rigdash-bridge")
    p.add_argument("--source", choices=sorted(SOURCES), default="demo")
    p.add_argument("--port", help="serial port (COM5, /dev/ttyACM0) or 'auto'")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--rate", type=float, default=30.0, help="frames per second")
    p.add_argument("--dry-run", action="store_true", help="print frames as hex instead of sending")
    p.add_argument("--frames", type=int, default=0, help="stop after N telemetry frames (0 = forever)")
    p.add_argument("--pages", type=parse_pages, default=parse_pages("all"),
                   help=f"comma list of pages to show: {','.join(PAGES)} (default all)")
    p.add_argument("--brightness", type=int, default=200, help="display brightness 0..255")
    p.add_argument("--units", type=parse_units, default=Units.METRIC,
                   help="comma list: metric, mph, fahrenheit, bar")
    p.add_argument("--log", metavar="CSV", help="append one row per completed lap to this CSV file")
    args = p.parse_args(argv)

    if not args.dry_run and not args.port:
        p.error("--port is required unless --dry-run is given (use --port auto to detect)")

    if args.dry_run:
        def write(frame: bytes) -> None:
            print(frame.hex(" ").upper())
    else:
        try:
            import serial  # type: ignore
        except ImportError:
            print("pyserial missing: pip install 'rigdash[serial]'", file=sys.stderr)
            return 2
        port_name = args.port
        if port_name == "auto":
            from .ports import find_display_port
            port_name = find_display_port()
            if not port_name:
                print("no display found; plug it in or pass --port", file=sys.stderr)
                return 2
            print(f"display on {port_name}", file=sys.stderr)
        port = serial.Serial(port_name, args.baud, timeout=0)
        write = port.write

    source = SOURCES[args.source]()
    config = Config(page_mask=args.pages, brightness=max(0, min(255, args.brightness)), units=args.units)
    scheduler = FrameScheduler(config, source.track_name())
    enricher = Enricher()
    logger = None
    if args.log:
        from .logger import SessionLogger
        logger = SessionLogger(args.log, source.track_name())

    period = 1.0 / args.rate
    sent = 0
    try:
        while args.frames == 0 or sent < args.frames:
            now = _now_ms()
            snap = source.poll(now)
            if snap is not None:
                enricher.apply(snap)
                if logger:
                    logger.update(snap)
                sent += 1
            for frame in scheduler.frames(snap, now):
                write(frame)
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    finally:
        source.close()
        if logger:
            logger.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
