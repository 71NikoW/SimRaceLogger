# Concept

Status: **draft / concept phase**. Hardware choice and screen layouts are open for discussion.

## Goal

An open-source, low-cost dash display for sim-racing rigs that shows the
information you actually look at while driving — current lap time, lap
number, position, fuel in laps, pit limiter, invalidated laps, engine state,
track map and weather — on a small wide screen mounted next to the wheel.

Anyone should be able to build one from off-the-shelf parts and a 3D-printed
case, and add support for new games without touching the firmware.

## Why not a plain Arduino (Uno/Nano)?

An ATmega328 has 2 KB RAM and a 16 MHz 8-bit core. That is fine for LEDs or
a 7-segment display, but far too little to drive a colour TFT with large,
smooth fonts at 20–30 fps. We use an **ESP32-S3** instead:

- still programmable with the Arduino framework / PlatformIO
- 240 MHz dual core, 512 KB SRAM + PSRAM for a full frame buffer
- native USB (CDC serial) — one cable for power and data
- Wi-Fi as an optional wireless link later

## Architecture

```
 ┌───────────────┐   shared memory /   ┌──────────────────┐   USB serial    ┌──────────────────┐
 │ Sim (ACC,     │   UDP / SDK         │ PC bridge        │   binary frames │ ESP32-S3 display │
 │ iRacing, LMU, ├────────────────────►│ (Python)         ├────────────────►│ firmware         │
 │ F1, AC, ...)  │                     │ game adapter →   │   20–30 Hz      │ parser → screen  │
 └───────────────┘                     │ normalized model │                 │ manager → render │
                                       └──────────────────┘                 └──────────────────┘
```

**Design rule:** all game-specific knowledge lives in the PC bridge. The
display only knows one small, versioned, game-agnostic protocol
([PROTOCOL.md](PROTOCOL.md)). New games = new Python adapter, no reflash.

### PC bridge (`bridge/`)

- `TelemetrySource` interface; one adapter per game
  (`demo` exists now; ACC, iRacing, LMU/rFactor 2, F1 UDP, Assetto Corsa planned).
- Normalizes everything into `Telemetry` (lap times in ms, flags, fuel in laps …).
- Encodes frames and sends them over serial at a fixed rate.
- Computes derived values the game does not provide (e.g. fuel laps
  from fuel per lap average, last pit stop duration).
- Optional later: SimHub plugin as an alternative to the standalone bridge.

### Firmware (`firmware/`)

Split into a **hardware-independent core** (`lib/core`) and a thin
**board layer** (`src/`):

| Core module      | Responsibility                                              | Tested on PC |
|------------------|-------------------------------------------------------------|--------------|
| `protocol`       | Streaming frame parser, CRC, payload decoding, resync       | yes          |
| `format`         | Lap time / delta / fuel / gear text formatting              | yes          |
| `screen_manager` | Which screen is visible (overlays, priorities, timeouts)    | yes          |

The board layer reads USB serial, feeds the parser, asks the screen manager
what to show and calls the renderer (LovyanGFX or LVGL, decided during the
prototype). Buttons / the two LEDs on top are handled there as well.

## Screens

Taken from the reference pictures:

| Screen            | Content                                                         | Shown when                        |
|-------------------|-----------------------------------------------------------------|-----------------------------------|
| Current lap       | Big running lap time, session type, position, fuel laps, lap no.| default page                      |
| Track data        | Track name, mini map with car position, fuel %, air/track temp  | page selected by button           |
| Last lap / delta  | Last lap, best lap, delta to best                                | page selected by button           |
| Race flag         | Full-screen flag (yellow, blue, black, chequered, SC, VSC …)    | 3 s after the flag changes        |
| Pit limiter       | Blinking green banner + gear, speed, position, last pit stop, flag | pit limiter active             |
| Lap invalidated   | Red warning                                                     | 2.5 s after lap becomes invalid   |
| Engine off        | "ENGINE OFF"                                                    | engine not running                |
| No signal         | "WAITING FOR TELEMETRY"                                         | no valid frame for > 2 s          |

Priority (highest first): no signal → race flag → lap invalidated →
pit limiter → engine off → selected page. While a flag is waved, every
screen shows a flag-coloured strip and the LEDs take the flag colour. This logic is in `screen_manager` and fully
unit-tested.

## Hardware (proposal, to be validated)

See [HARDWARE.md](HARDWARE.md).

## Test strategy

1. **Host unit tests (now):** firmware core compiled with g++ via CMake,
   bridge tested with pytest. Run in CI on every push.
2. **Golden vectors:** `protocol/vectors.txt` is generated by the Python
   encoder and decoded by the C++ tests — guarantees both sides speak the
   same protocol.
3. **Bridge dry-run:** `rigdash-bridge --source demo --dry-run` prints frames
   without hardware.
4. **Hardware-in-the-loop (later):** demo source → real display, check
   frame rate, latency (< 50 ms target) and reconnect behaviour.

## Roadmap

1. ✅ Concept, protocol v1, core logic + tests, demo bridge
2. Prototype on a dev board: renderer for "current lap" screen
3. Game adapters: ACC + iRacing first (most requested), then LMU, F1, AC
4. All screens, button page switching, LED usage (e.g. shift / flags)
5. 3D-printable case (STEP + STL), build guide, wiring photos
6. Optional: Wi-Fi link, SimHub plugin, web configurator
