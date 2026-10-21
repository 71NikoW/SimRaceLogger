# Backlog / ideas

Brainstorm, not yet prioritised. ✅ = done.

## Race information
- ✅ Flags incl. safety car and virtual safety car / full course yellow
- Penalties: drive-through, stop & go, time penalty, track-limit warnings ("3/4")
- Gap ahead / behind and opponent lap times (race only)
- Laps remaining / session time remaining, "last lap" indicator
- Blue-flag warning with gap to the faster car
- Pit window open/closed, mandatory stop done

## Car
- Shift lights on the two LEDs (or an optional WS2812 strip), per-car RPM
- Fuel: per-lap average, fuel to add to finish, low-fuel warning
- Tyres: temperatures, pressures, wear; brake temps
- ABS / TC / brake bias / engine map — popup when changed on the wheel
- Damage warning

## Timing
- Live delta bar to best / session best / optimal lap
- Sector times with colour (purple / green / yellow)
- Theoretical best lap from best sectors
- Lap history list (last 5 laps)

## Device & UX
- Brightness button + auto-dim at night
- Configurable pages and priorities (web configurator over USB / Wi-Fi)
- Themes / colour schemes, unit switch (km/h · mph, °C · °F)
- Boot screen with firmware + protocol version
- Firmware update via the bridge (no Arduino IDE for end users)

## Software
- Game adapters: ACC, iRacing, LMU / rFactor 2, F1 25, AC / AC EVO, AMS2, RaceRoom
- SimHub plugin as an alternative to the standalone bridge
- Session logging to CSV (lap times, fuel, flags) — the "logger" part of the repo name
- Tray app with auto-detect of game and COM port
- Multiple displays on one bridge (e.g. one for timing, one for car)

## Hardware & community
- 3D-printable case variants (desk stand, wheel-base mount, button box insert)
- Build guide with photos, wiring diagram, BOM with links
- Option for a cheap ESP32-S3 all-in-one board
- Contributing guide, issue templates, example videos
