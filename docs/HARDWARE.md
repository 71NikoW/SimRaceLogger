# Hardware (proposal)

Nothing here is final — the first prototype decides. Prices are rough.

## Option A — all-in-one board (recommended for beginners)

An ESP32-S3 board with an integrated wide or 3.5" IPS display. No soldering
for the display, just USB-C. Look for:

- ESP32-S3 with **8 MB PSRAM** (frame buffer)
- IPS panel, ≥ 480×320 or a wide "bar" format (e.g. 640×172 / 480×222)
- supported by LovyanGFX or LVGL
- ~20–35 €

## Option B — dev board + separate display

| Part                                    | Notes                         | ~Price |
|-----------------------------------------|-------------------------------|--------|
| ESP32-S3 DevKitC-1 N8R8                 | native USB                    | 10 €   |
| 3.5" IPS SPI display, ST7796 or ILI9488 | 480×320                       | 12 €   |
| 2× tactile button                       | page switch / brightness      | 1 €    |
| 2× 3 mm LED + 220 Ω resistors           | status LEDs on top of case    | 1 €    |
| USB-C cable                              |                               | 3 €    |
| 3D-printed case (PETG)                   | STL in `case/` (planned)      | —      |

### Wiring sketch (Option B, SPI)

| Display | ESP32-S3 |
|---------|----------|
| VCC     | 3V3      |
| GND     | GND      |
| SCK     | GPIO 12  |
| MOSI    | GPIO 11  |
| CS      | GPIO 10  |
| DC      | GPIO 9   |
| RST     | GPIO 8   |
| BL      | GPIO 7 (PWM) |

Buttons: GPIO 4 / GPIO 5 to GND (internal pull-up). LEDs: GPIO 15 / 16.
Pins are placeholders until the prototype is verified.

## Mounting

Case with the two LED holes on top (as in the reference pictures), 1/4"
thread or two M4 holes on the back for the usual rig profile brackets.
