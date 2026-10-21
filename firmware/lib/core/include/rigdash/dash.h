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

// --- LED frame (WS2812B / SK6812 strip around the display) -----------------
// Index order follows the wiring of the LED bars (hardware/ledbar): one
// chain up the left side (bottom→top), across the top (left→right) and down
// the right side (top→bottom). The driver scales by brightness.
struct Rgb {
  uint8_t r = 0, g = 0, b = 0;
  bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
  bool off() const { return r == 0 && g == 0 && b == 0; }
};
struct StripLayout {
  uint8_t top = 12;   // shift lights
  uint8_t side = 4;   // per side: flags, warnings, delta
  constexpr uint16_t count() const { return top + 2u * side; }
  // Chain index of LED i (0 = top) on each side, and of top LED i (0 = left).
  constexpr uint16_t left(uint8_t i) const { return static_cast<uint16_t>(side - 1 - i); }
  constexpr uint16_t topAt(uint8_t i) const { return static_cast<uint16_t>(side + i); }
  constexpr uint16_t right(uint8_t i) const { return static_cast<uint16_t>(side + top + i); }
};
namespace rgb {
constexpr Rgb kOff{0, 0, 0}, kGreen{0, 255, 0}, kYellow{255, 190, 0}, kRed{255, 0, 0},
    kBlue{0, 60, 255}, kCyan{0, 200, 255}, kWhite{255, 255, 255}, kOrange{255, 90, 0},
    kPurple{170, 0, 255}, kDimBlue{0, 10, 45}, kDimGreen{0, 40, 0}, kDimRed{45, 0, 0};
}

enum class AlertKind : uint8_t;
// Fills `out` (layout.count() entries, chain order; top/side max 64 each).
//  top:   shift lights green → yellow → red, all flash blue at the shift
//         point; pit limiter = alternating halves blue; no signal = scanner.
//  sides: flag colour (blinking; chequered alternates), pit limiter green,
//         lap invalid red, alert colour by category, otherwise dim delta
//         colour (green faster / red slower) or dim blue.
void stripFor(Screen screen, const State& s, AlertKind alert, uint32_t now_ms,
              const StripLayout& layout, Rgb* out);
Rgb alertColour(AlertKind kind);

// Brightness (0..255) that keeps `n` LEDs showing `px` under `max_ma`, at most
// `wanted`. Assumes WS2812B: ~20 mA per colour channel at full, ~1 mA idle.
uint8_t limitBrightness(const Rgb* px, uint16_t n, uint8_t wanted, uint16_t max_ma);

}  // namespace rigdash
