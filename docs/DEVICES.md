# Extra devices

Same bridge, same protocol, same look as the lap display. Every device is an
ESP32-S3 on USB; the bridge sends the telemetry stream to all of them
(`rigdash-bridge --port auto` finds every connected device).

Status: logic and tests done (`firmware/lib/core`), board code compile-checked,
cases still to design. Try both in the simulator.

## Flag panel

A 16×16 LED matrix behind a frosted front that shows the race flag big
enough to catch from the corner of your eye.

| Flag | Panel |
|------|-------|
| Green | full green for 3 s, then dark |
| Yellow | full yellow, blinking |
| Blue | blue with a yellow diagonal, blinking |
| Safety car / VSC | blinking yellow with "SC" / "VSC" |
| Chequered | 4×4 chequer pattern that swaps — looks like waving |
| Red / white | solid |
| Black | blinking white frame (a black flag can't glow) |
| Pit limiter (no flag) | blinking green frame |
| No flag | dark — or dim green / red delta colour (option) |

Parts (~30 €): ESP32-S3 DevKitC-1 (or any ESP32-S3 with native USB),
WS2812B 16×16 flexible matrix (160 × 160 mm), opal acrylic or a printed
white diffuser, USB-C cable. Matrix data on GPIO5, 5 V and GND from the
board's 5 V pin. The firmware limits the current to 450 mA (USB 2.0), so the
panel is bright enough indoors but not at full LED power; for more, feed the
matrix from its own 5 V supply.

Build: `pio run -e flagpanel -t upload`.

## Button box

Shows up in Windows as a game controller — map the buttons in any sim — and
receives telemetry over the same USB cable so the buttons light up.

| Control | Gamepad buttons | LED |
|---------|-----------------|-----|
| 12 push buttons (START, PIT, FLAG, 9 free) | 1–12 | START blinks red when ignition is on and the engine off; PIT blinks green with the limiter on; FLAG shows the flag; others dim backlight, bright while pressed |
| 2 toggle switches (ignition, lights) | 13–14, one short press per flip | ignition: red = on, green = engine running |
| 4 rotary encoders (ABS, TC, MAP, BB) | 17–24, one press per detent (CW, CCW) | — |

Parts (~45 €): ESP32-S3 DevKitC-1, 12× 16 mm or 19 mm push buttons (with
transparent caps for the LEDs), 2 toggle switches (one with a flip cover for
ignition), 4× EC11 rotary encoders + knobs, 14 WS2812B LEDs (strip pieces or
the LED bar PCBs), wire. Pins are in `firmware/src/buttonbox_main.cpp`.

Build: `pio run -e buttonbox -t upload`.

## Next

- Cases for the A1 mini (180 mm bed): flag panel 170 × 170 mm frame, button box
  ~170 × 100 mm, both with 40-series profile and 1/4" camera mounts.
