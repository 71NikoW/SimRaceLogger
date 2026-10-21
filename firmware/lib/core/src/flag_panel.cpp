#include "rigdash/flag_panel.h"

#include "rigdash/screen_manager.h"

namespace rigdash {

uint16_t serpentineIndex(uint8_t x, uint8_t y, uint8_t width) {
  return static_cast<uint16_t>(y * width + ((y & 1) ? (width - 1 - x) : x));
}

void FlagPanel::onTelemetry(const Telemetry& t, uint32_t now) {
  if (t.race_flag == RaceFlag::Green && (!have_ || last_.race_flag != RaceFlag::Green)) {
    green_since_ = now;
  }
  last_ = t;
  have_ = true;
  rx_ms_ = now;
}

PanelMode FlagPanel::mode(uint32_t now) const {
  if (!have_ || now - rx_ms_ > kSignalTimeoutMs) return PanelMode::NoSignal;
  const RaceFlag f = last_.race_flag;
  if (f == RaceFlag::Green) {
    if (now - green_since_ < kGreenFlashMs) return PanelMode::Flag;
  } else if (f != RaceFlag::None) {
    return PanelMode::Flag;
  }
  if (last_.has(kFlagPitLimiter)) return PanelMode::PitLimiter;
  if (delta_idle_ && last_.best_lap_ms != 0) return PanelMode::Delta;
  return PanelMode::Off;
}

void FlagPanel::fill(Rgb* out, Rgb c) const {
  for (uint16_t i = 0; i < static_cast<uint16_t>(w_) * h_; ++i) out[i] = c;
}

namespace {
// 3×5 glyphs, one row per byte (bit 2 = left column).
struct Glyph {
  char c;
  uint8_t rows[5];
};
constexpr Glyph kFont[] = {
    {'S', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'C', {0b111, 0b100, 0b100, 0b100, 0b111}},
    {'V', {0b101, 0b101, 0b101, 0b101, 0b010}},
};
const Glyph* glyph(char c) {
  for (const Glyph& g : kFont) {
    if (g.c == c) return &g;
  }
  return nullptr;
}
}  // namespace

void FlagPanel::drawText(Rgb* out, const char* s, Rgb c) const {
  // Scale 2× when it fits, centred.
  int n = 0;
  while (s[n]) ++n;
  const int scale = (n * 4 - 1) * 2 <= w_ && 10 <= h_ ? 2 : 1;
  const int tw = (n * 4 - 1) * scale;
  const int th = 5 * scale;
  const int x0 = (w_ - tw) / 2;
  const int y0 = (h_ - th) / 2;
  for (int i = 0; i < n; ++i) {
    const Glyph* g = glyph(s[i]);
    if (!g) continue;
    for (int r = 0; r < 5; ++r) {
      for (int col = 0; col < 3; ++col) {
        if (!((g->rows[r] >> (2 - col)) & 1)) continue;
        for (int dy = 0; dy < scale; ++dy) {
          for (int dx = 0; dx < scale; ++dx) {
            const int x = x0 + (i * 4 + col) * scale + dx;
            const int y = y0 + r * scale + dy;
            if (x >= 0 && x < w_ && y >= 0 && y < h_) out[y * w_ + x] = c;
          }
        }
      }
    }
  }
}

void FlagPanel::render(uint32_t now, Rgb* out) const {
  fill(out, rgb::kOff);
  const bool blink = blinkOn(now);
  switch (mode(now)) {
    case PanelMode::Off:
      return;
    case PanelMode::NoSignal: {
      const uint32_t n = static_cast<uint32_t>(w_) * h_;
      out[(now / 150u) % n] = rgb::kDimBlue;
      return;
    }
    case PanelMode::PitLimiter:
      if (!blink) return;
      for (uint8_t x = 0; x < w_; ++x) out[x] = out[(h_ - 1) * w_ + x] = rgb::kGreen;
      for (uint8_t y = 0; y < h_; ++y) out[y * w_] = out[y * w_ + w_ - 1] = rgb::kGreen;
      return;
    case PanelMode::Delta:
      fill(out, last_.delta_ms <= 0 ? rgb::kDimGreen : rgb::kDimRed);
      return;
    case PanelMode::Flag:
      break;
  }
  switch (last_.race_flag) {
    case RaceFlag::Green:
      fill(out, rgb::kGreen);
      return;
    case RaceFlag::Yellow:
      if (blink) fill(out, rgb::kYellow);
      return;
    case RaceFlag::Red:
      fill(out, rgb::kRed);
      return;
    case RaceFlag::White:
      fill(out, rgb::kWhite);
      return;
    case RaceFlag::Blue:
      if (!blink) return;
      fill(out, rgb::kBlue);
      // Yellow diagonal stripe from bottom-left to top-right, 2 px wide.
      for (uint8_t y = 0; y < h_; ++y) {
        const int x = (h_ - 1 - y) * w_ / h_;
        for (int k = -1; k <= 1; ++k) {
          if (x + k >= 0 && x + k < w_) out[y * w_ + x + k] = rgb::kYellow;
        }
      }
      return;
    case RaceFlag::Black:
      // A black flag can't glow: white frame around a dark centre, blinking.
      if (!blink) return;
      for (uint8_t x = 0; x < w_; ++x) out[x] = out[(h_ - 1) * w_ + x] = rgb::kWhite;
      for (uint8_t y = 0; y < h_; ++y) out[y * w_] = out[y * w_ + w_ - 1] = rgb::kWhite;
      return;
    case RaceFlag::Chequered: {
      // 4×4 squares that swap every half second: looks like a waving flag.
      const uint8_t sq = w_ >= 8 ? w_ / 4 : 1;
      for (uint8_t y = 0; y < h_; ++y) {
        for (uint8_t x = 0; x < w_; ++x) {
          const bool white = (((x / sq) + (y / sq)) & 1) == (blink ? 1 : 0);
          out[y * w_ + x] = white ? rgb::kWhite : rgb::kOff;
        }
      }
      return;
    }
    case RaceFlag::SafetyCar:
    case RaceFlag::VirtualSafetyCar:
      fill(out, blink ? rgb::kYellow : rgb::kOff);
      drawText(out, last_.race_flag == RaceFlag::SafetyCar ? "SC" : "VSC", blink ? rgb::kOff : rgb::kYellow);
      return;
    case RaceFlag::None:
      return;
  }
}

}  // namespace rigdash
