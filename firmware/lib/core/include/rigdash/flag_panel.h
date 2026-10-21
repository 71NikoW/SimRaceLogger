#pragma once
// Flag panel: a WS2812B matrix (default 16×16) showing the race flag big
// enough to catch from the corner of your eye. Pure logic, unit-tested.
#include <stdint.h>

#include "rigdash/dash.h"
#include "rigdash/telemetry.h"

namespace rigdash {

constexpr uint32_t kGreenFlashMs = 3000;   // green is shown briefly, then dark
constexpr uint8_t kPanelMax = 32;          // max width / height

// Physical index of pixel (x, y) on a serpentine-wired matrix (rows wired
// left→right, then right→left, ...; row 0 at the top).
uint16_t serpentineIndex(uint8_t x, uint8_t y, uint8_t width);

enum class PanelMode : uint8_t {
  Off,         // nothing to show
  NoSignal,    // slow scanner
  Flag,        // race flag
  PitLimiter,  // green frame blinking
  Delta,       // optional idle: whole panel dim green / red
};

class FlagPanel {
 public:
  FlagPanel(uint8_t width = 16, uint8_t height = 16) : w_(width), h_(height) {}
  void onTelemetry(const Telemetry& t, uint32_t now_ms);
  void setDeltaIdle(bool on) { delta_idle_ = on; }
  PanelMode mode(uint32_t now_ms) const;
  // Fills `out` with w×h pixels in logical order (row-major, y = 0 at top).
  void render(uint32_t now_ms, Rgb* out) const;
  uint8_t width() const { return w_; }
  uint8_t height() const { return h_; }

 private:
  void fill(Rgb* out, Rgb c) const;
  void drawText(Rgb* out, const char* s, Rgb c) const;

  uint8_t w_, h_;
  Telemetry last_;
  bool have_ = false;
  uint32_t rx_ms_ = 0;
  uint32_t green_since_ = 0;
  bool delta_idle_ = false;
};

}  // namespace rigdash
