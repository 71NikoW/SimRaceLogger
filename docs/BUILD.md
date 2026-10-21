# Build guide

How to turn this project into a physical display on your rig.

> **Status:** protocol, logic, LEDs and buttons are done and tested; the LED
> bar PCBs pass KiCad's design rule check and the case is generated. Drawing
> the screens on the panel needs the real board on the desk (next step).
> **Display dimensions in `hardware/case/build.py` are not measured yet** —
> measure your board before printing the full case.

## Shopping list

### Buy

| Part | Qty | Notes | ~Price |
|------|-----|-------|--------|
| **Waveshare ESP32-S3-Touch-LCD-4.3** (not the "4.3B") | 1 | ESP32-S3, 8 MB PSRAM, 800×480 IPS, capacitive touch | 30–40 € |
| USB-C cable with data lines, 2 m | 1 | power + telemetry | 5–8 € |
| M3 heat-set inserts (4.0 mm hole, ≤ 8 mm long) | 4 | | 2 € |
| M3×8 screws (cylinder or countersunk head) | 4 | | 1 € |
| 1/4"-20 hex nut (camera thread) | 1 | for a ball-head / camera arm | 1 € |
| Silicone wire 26 AWG, 3 colours | ~50 cm | LED bars to board | 3 € |
| Camera ball-head arm with 1/4" screw | 1 | or anything with a 1/4" thread | 8–15 € |

### Order at PCBWay

| Item | Files | Notes |
|------|-------|-------|
| LED bar **top**, assembled | `hardware/ledbar/out/top/` | 5 pcs minimum — 1 per display |
| LED bar **side**, assembled | `hardware/ledbar/out/side/` | 2 per display |
| Case (3D printing, optional) | `hardware/case/out/*.stl` | or print it yourself |

Settings and upload steps: [hardware/ledbar/README.md](../hardware/ledbar/README.md).

### Print (own printer or PCBWay 3D printing)

See [hardware/case/README.md](../hardware/case/README.md): `test_frame.stl`
first, then front, back and diffusers. PETG or ASA, diffusers in white PETG.

### Tools

Soldering iron, solder, side cutter, multimeter, a PC with VS Code +
PlatformIO (or Arduino IDE 2 with the ESP32 core 3.x).

## Wiring

```
 PC ──USB-C──► Waveshare ESP32-S3 4.3"
                 │ GPIO6 (sensor connector) ── D ─┐
                 │ 5V ───────────────────────── 5V┤ IN  left bar  (bottom → top)
                 │ GND ──────────────────────── G ┘     │ OUT → IN top bar (left → right)
                 │                                      │ OUT → IN right bar (top → bottom)
                 └ touch: tap = next page, long press = previous
```

No level shifter needed: the first LED on each side bar runs slightly
below 5 V so it accepts the ESP32's 3.3 V data (see the LED bar README).

**Power:** USB gives 500 mA. 20 LEDs at full white would take ~1.2 A, so the
firmware caps LED brightness (`kStripMaxBrightness` in `firmware/src/main.cpp`).

Pins are placeholders until verified on the real board (`kPinStrip` in
`firmware/src/main.cpp`).

## Steps

1. **Measure the board** (caliper): outline, thickness, active area position,
   USB-C position. Put the numbers into `hardware/case/build.py`, run
   `python3 hardware/case/build.py`, print `test_frame.stl` and check the fit.
2. **Order the LED bars** at PCBWay (assembled), print the case.
3. **Flash the board:**
   ```bash
   git clone https://github.com/71NikoW/SimRaceLogger.git && cd SimRaceLogger/firmware
   pio run -t upload
   ```
4. **Test without a game:**
   ```bash
   pip install -e "bridge[serial]"
   rigdash-bridge --source spa --port auto
   ```
5. **Wire the LED bars** (3 wires between each, see above) and test again:
   shift lights on top, yellow at Pouhon, alternating sides under the safety car.
6. **Assemble:** inserts into the bosses, diffusers into the recesses, LED bars
   behind the LED windows (double-sided tape), display on top, lid on.
7. **Mount** in your line of sight next to the wheel, strain-relieve the cable.
8. **Racing:** start `rigdash-bridge --source <game> --port auto` before the
   game (game adapters are in progress — see [BACKLOG.md](BACKLOG.md)).
   `--log laps.csv` records every lap.

## Options

- **Units / pages / brightness:** `--units mph,fahrenheit`,
  `--pages current,delta,tyres,weather`, `--brightness 120`.
- **Different LED count:** change `kStripLayout{12, 4}` in
  `firmware/src/main.cpp` and the bars in `hardware/ledbar/build.py`.
- **Cheaper without PCBs:** a WS2812B strip (144 LEDs/m) cut into 12 + 4 + 4,
  wired in the same order, plus a 74AHCT125 level shifter on the data line.
