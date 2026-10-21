# Build guide

How to turn this project into a physical display on your rig.

> **Status:** protocol, logic, LEDs and buttons are done and tested. Drawing
> the screens on the real panel is the next step and needs the actual board
> on the desk (driver choice: LovyanGFX or LVGL). Until then the firmware
> prints each screen as text on the debug port and drives the LEDs.
> Pins below are placeholders until the first prototype is verified.

## What you need

### Electronics

| Part | Why | ~Price |
|------|-----|--------|
| **ESP32-S3 board with a 4.3" IPS display, 800×480** (e.g. the "ESP32-8048S043" type or Waveshare's ESP32-S3 4.3" board) | Brain + screen in one, no display wiring. Must have **ESP32-S3** and **8 MB PSRAM**; IPS for viewing angles | 25–40 € |
| **WS2812B LED strip, 144 LEDs/m, 5 V** (or SK6812, same wiring) | LED frame: 12 on top for shift lights, 4 down each side for flags and warnings — cut 20 LEDs | 8–12 € |
| 74AHCT125 level shifter (or SN74AHCT1G125) | LEDs want 5 V data, the ESP32 outputs 3.3 V; often works without, reliably with | 1–2 € |
| 330 Ω resistor + 1000 µF / 10 V capacitor | Protects the first LED / smooths the 5 V rail | 1 € |
| 2× momentary push button (12 mm panel mount) | Page (long press = back) and brightness | 2–4 € |
| USB-C cable, 2 m, **with data lines** | Power + telemetry from the PC | 5–8 € |
| Silicone wire 26 AWG, heat shrink, optional JST-SM 3-pin plugs | LED chain and buttons | 3–5 € |

Budget: **about 45–75 €** plus the case.

Cheaper alternative: ESP32-S3 DevKitC-1 (N8R8) + 3.5" SPI IPS module
(ST7796 / ILI9488, 480×320). Needs 8 wires to the display and shows a
smaller picture (480×216 window).

### Case

| Part | Notes |
|------|-------|
| 3D-printed front frame, back cover, LED diffusers | PETG or ASA (PLA can soften in a hot room / car-cockpit sun). Diffusers in white/natural PETG, 0.8–1.2 mm, or a strip of opal acrylic. ~80 g filament. No printer? Any online print service. |
| 4–6× M3 heat-set inserts + M3×8 screws | Holds front and back together |
| Mount: 1/4"-20 nut (camera thread) **or** 2× M6 T-nuts for 40-series aluminium profile | Ball-head arm from a camera mount works well |

Case files (STL/STEP) are not in the repo yet — they come after the
prototype fixes the board's exact dimensions.

### Tools

Soldering iron, solder, side cutter, multimeter, a PC with VS Code +
PlatformIO (or Arduino IDE 2 with the ESP32 core 3.x).

## Wiring

```
                   ┌──────────── LED frame (one chain) ────────────┐
 PC ──USB-C──► ESP32-S3 ── GPIO21 ─► 74AHCT125 ─► 330 Ω ─► DIN  top 1 … top 12
                   │ 5V ───────────────────────────────── 5V        │ (wire)
                   │ GND ──────────────────────────────── GND   left 1 … left 4 (top→bottom)
                   │                                                 │ (wire)
                   │ GPIO4 ── button "page" ── GND             right 1 … right 4 (top→bottom)
                   │ GPIO5 ── button "brightness" ── GND
                   └ 1000 µF across 5V/GND at the strip input
```

| From | To | Note |
|------|----|------|
| ESP32 GPIO21 | 74AHCT125 A, Y → 330 Ω → strip DIN | 74AHCT125 powered from 5 V, OE to GND |
| ESP32 5V (USB) | strip 5V, 74AHCT125 VCC | |
| ESP32 GND | strip GND, buttons, 74AHCT125 GND | |
| GPIO4 | page button → GND | internal pull-up |
| GPIO5 | brightness button → GND | internal pull-up |

LED order matters: the firmware expects **top left→right, then left side
top→bottom, then right side top→bottom**, as one chain. Follow the arrows
printed on the strip (DIN → DOUT) and bridge the gaps with three short wires.

**Power:** USB gives 500 mA. 20 LEDs at full white would take ~1.2 A, so the
firmware caps LED brightness (`kStripMaxBrightness` in `firmware/src/main.cpp`).
If you want them brighter, feed the strip from a separate 5 V supply
(common GND!).

## Steps

1. **Flash the board.** Plug it in, then:
   ```bash
   git clone https://github.com/71NikoW/SimRaceLogger.git && cd SimRaceLogger/firmware
   pio run -t upload          # VS Code: PlatformIO → Upload
   ```
2. **Try it without a game.** On the PC:
   ```bash
   pip install -e "bridge[serial]"
   rigdash-bridge --source spa --port auto
   ```
   The board receives the scripted Spa race. Watch the debug output with
   `pio device monitor` (UART0) and the LEDs.
3. **Solder the LED chain** (cut 12 + 4 + 4 from the strip at the copper
   pads), add level shifter, resistor and capacitor, test with step 2 —
   the top bar should run through the shift lights, the sides should blink
   yellow at Pouhon and alternate under the safety car.
4. **Buttons:** solder to GPIO4 / GPIO5 and GND.
5. **Case:** press in heat-set inserts, glue diffusers into the LED slots,
   fit display and LED strips, close with M3 screws.
6. **Mount** within your line of sight next to the wheel (ideally below the
   monitor's lower edge, like in the reference photos), route the USB cable
   with strain relief.
7. **Racing:** start `rigdash-bridge --source <game> --port auto` before the
   game (game adapters are in progress — see [BACKLOG.md](BACKLOG.md)).
   `--log laps.csv` records every lap.

## Options

- **Units / pages / brightness:** `--units mph,fahrenheit`,
  `--pages current,delta,tyres,weather`, `--brightness 120`.
- **More LEDs:** change `kStripLayout{12, 4}` in `firmware/src/main.cpp`
  (e.g. `{16, 6}`); everything else adapts.
- **Two simple status LEDs instead of a frame:** RGB LEDs on the pins in
  `kPinLed` — the firmware drives both.
