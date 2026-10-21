// Flag panel: USB serial -> FrameParser -> FlagPanel -> 16×16 WS2812B matrix.
// Build: pio run -e flagpanel. Any ESP32-S3 board with native USB works.
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "rigdash/flag_panel.h"
#include "rigdash/protocol.h"

using namespace rigdash;

namespace {

constexpr int kPinMatrix = 5;
constexpr uint8_t kW = 16, kH = 16;
constexpr uint8_t kBrightness = 120;   // wanted; the limiter may go lower
constexpr uint16_t kMaxMilliamps = 450; // stay under USB 2.0's 500 mA
constexpr uint32_t kFrameMs = 33;

FrameParser parser;
State state;
FlagPanel panel(kW, kH);
Adafruit_NeoPixel matrix(kW * kH, kPinMatrix, NEO_GRB + NEO_KHZ800);
Rgb px[kW * kH];

}  // namespace

void setup() {
  Serial.begin(115200);
  matrix.begin();
  matrix.show();
}

void loop() {
  const uint32_t now = millis();
  while (Serial.available() > 0) {
    parser.push(static_cast<uint8_t>(Serial.read()));
    while (parser.next()) {
      if (applyFrame(parser.frame(), state) == kMsgTelemetry) panel.onTelemetry(state.telemetry, now);
    }
  }
  static uint32_t last = 0;
  if (now - last < kFrameMs) return;
  last = now;
  panel.render(now, px);
  matrix.setBrightness(limitBrightness(px, kW * kH, kBrightness, kMaxMilliamps));
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const Rgb& c = px[y * kW + x];
      matrix.setPixelColor(serpentineIndex(x, y, kW), c.r, c.g, c.b);
    }
  }
  matrix.show();
}
