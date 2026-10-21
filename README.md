# X30 Race Session Logger — open-source rig lap display

A small dash display for sim-racing rigs: big current lap time, lap number,
position, fuel in laps, pit limiter, invalidated laps, engine state, track
map and weather. ESP32-S3 + TFT, 3D-printed case, PC bridge for the sim.

> **Want to build one?** See the [build guide](docs/BUILD.md) for parts and wiring.
>
> **Status: concept phase.** Protocol, core logic and tests exist; display
> rendering, game adapters and the case are next. See the
> [concept](docs/CONCEPT.md).

```
Sim  ──►  PC bridge (Python)  ──USB serial──►  ESP32-S3 display
          game adapters                        parser · screen logic · renderer
```

## Repository layout

| Path               | What                                                         |
|--------------------|--------------------------------------------------------------|
| `docs/`            | [Concept](docs/CONCEPT.md), [protocol](docs/PROTOCOL.md), [build guide](docs/BUILD.md), [backlog](docs/BACKLOG.md) |
| `bridge/`          | PC side: telemetry sources → serial frames (`rigdash-bridge`) |
| `firmware/lib/core`| Hardware-independent C++: protocol parser, formatting, screen manager |
| `firmware/src`     | ESP32-S3 board layer (PlatformIO, Arduino framework)         |
| `protocol/`        | Golden test frames shared by Python and C++ tests            |
| `simulator/`       | Browser preview of all screens — open `simulator/index.html` |

## Run the tests

```bash
# PC bridge
pip install -e "bridge[dev]"
pytest bridge

# Firmware core on your PC (no board needed)
cmake -S firmware -B build && cmake --build build && ctest --test-dir build
```

## Try the bridge without a sim

```bash
rigdash-bridge --source demo --dry-run --frames 10       # print frames
rigdash-bridge --source spa --port auto                  # scripted Spa race with flags
pip install -e "bridge[serial]"
rigdash-bridge --source demo --port auto                 # find the display and send
rigdash-bridge --port COM5 --pages current,delta,tyres,weather --units mph --log laps.csv
```

`--log` appends one CSV row per completed lap (time, sectors, fuel used,
weather) — open it in Excel or any spreadsheet.

## Flash the firmware

```bash
cd firmware && pio run -t upload
```

## Contributing

Game adapters are the easiest way to help: implement
`rigdash.sources.TelemetrySource` for your sim (return a `Snapshot`; fill in
what the game exposes, the bridge derives fuel per lap, sectors and lap history) and register it in
`bridge/src/rigdash/sources/__init__.py`. Protocol changes must update
`docs/PROTOCOL.md`, both implementations and `protocol/vectors.txt`.

MIT licensed.
