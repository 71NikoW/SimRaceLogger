#pragma once
// Small pure helpers the renderer and LED driver need. All unit-tested.
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

// Shift lights: how many of `n` LEDs are lit. Lighting starts at 90 % of
// shift_rpm; all are lit at shift_rpm. 0 when shift_rpm is unknown.
uint8_t shiftLedsLit(const Engine& e, uint8_t n);
// True when the driver is at/over the shift point (LEDs should flash).
bool shiftNow(const Engine& e);

// Delta bar offset in pixels from the centre, negative (= faster) to the
// left. Full deflection at ±range_ms, clamped.
int16_t deltaBarPx(int32_t delta_ms, int16_t half_width_px, int32_t range_ms = 2000);

enum class TempBand : uint8_t { Unknown, Cold, Optimal, Hot };
// Tyre temperature against the compound's optimal window.
TempBand tyreBand(int16_t temp_x10, int16_t opt_min_x10, int16_t opt_max_x10);
// Brake temperature: cold < 150 °C, hot > 800 °C (typical GT values).
TempBand brakeBand(int16_t temp_c);

enum class Led : uint8_t { Off, Blue, Green, Yellow, Red, White, Purple };
struct Leds {
  Led left = Led::Off;
  Led right = Led::Off;
};

enum class Screen : uint8_t;
// Colours of the two status LEDs. Priority: pit limiter (blink green) →
// race flag → shift lights → idle blue.
Leds ledsFor(Screen screen, const State& s, uint32_t now_ms);

}  // namespace rigdash
