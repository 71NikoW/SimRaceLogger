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
| `screen_manager` | Which screen is visible (overlays, priorities, page mask)   | yes          |
| `alerts`         | Edge-triggered popups: weather, fuel, damage, penalties, ABS/TC/BB changes | yes |
| `dash`           | Shift lights, LED colours, delta bar, tyre/brake temp bands | yes          |

The board layer reads USB serial, feeds the parser, asks the screen manager
what to show and calls the renderer (LovyanGFX or LVGL, decided during the
prototype). Buttons / the two LEDs on top are handled there as well.

## Screens

**Pages** (button cycles through them; the bridge's `--pages` picks which):

| Page         | Content                                                                    |
|--------------|----------------------------------------------------------------------------|
| Current lap  | Big running lap time, session, position, fuel laps, lap number             |
| Delta        | Live delta bar to best lap, sector times coloured purple / green / yellow  |
| Tyres        | Per wheel: temperature (cold / optimal / hot), pressure, wear, brake temp  |
| Car          | Fuel (L, per lap, laps left, to finish), ABS, TC, TC cut, map, brake bias, damage |
| Race         | Gap ahead / behind, time or laps left, penalty, track limits, pit window   |
| Weather      | Rain now, in 10 and 30 min, track wetness, grip, wind, temperatures        |
| Track data   | Track name, mini map with car position, fuel %, air / track temperature    |
| Laps         | Last, best, optimal lap and the last 5 laps (invalid ones marked)          |

**Overlays** (automatic, highest priority first):

| Overlay          | Shown when                                                        |
|------------------|-------------------------------------------------------------------|
| No signal        | no valid frame for > 2 s                                          |
| Race flag        | 3 s after the flag changes (yellow, blue, SC, VSC, chequered …)   |
| Lap invalidated  | 2.5 s after the lap becomes invalid                               |
| Alert popup      | weather warning, low fuel, fuel short, damage, penalty, track limits, last lap, pit window open (3 s); ABS / TC / map / brake bias changed (1.5 s) |
| Pit limiter      | limiter on — blinking banner + gear, speed, position, last stop, flag |
| Engine off       | engine not running                                                |

While a flag is waved every screen shows a flag-coloured strip.

**Weather warnings:** rain expected in 10 min · rain started · rain getting
heavier · track drying · wet tyres advised (on slicks in medium rain or a
wet track). Each fires once per change, not every frame.

**LED frame** (WS2812B chain, default 12 on top + 4 per side):

- top bar = shift lights: green → yellow → red, all flash blue at the shift
  point; alternating halves blue with the pit limiter; a slow scanner while
  waiting for telemetry
- sides = flag colour (blinking; safety car / VSC / chequered alternate
  left and right), pit limiter green, invalid lap red, warning colour
  (weather cyan, fuel/penalty orange, damage red, track limits yellow,
  setting change blue), otherwise dim green/red for the live delta

Builds with just **two status LEDs** get the same priorities in simple form.

**Demo sources:** `demo` (free play, endless laps, weather cycle) and `spa`
— a scripted 6-lap race at Spa-Francorchamps with pit exit, track limits at
Raidillon, yellow at Pouhon, blue flag at Blanchimont, a safety-car lap with
rain arriving, last lap and the chequered flag.

## Hardware (proposal, to be validated)

See [BUILD.md](BUILD.md) — parts list, wiring and steps.

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
