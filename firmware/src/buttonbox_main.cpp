// Button box: buttons, toggle switches and encoders as a USB gamepad; LEDs
// behind the buttons follow the telemetry the bridge sends over the same
// cable (USB CDC). Build: pio run -e buttonbox.
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <USB.h>
#include <USBHIDGamepad.h>

#include "rigdash/buttonbox.h"
#include "rigdash/protocol.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

namespace {

// Pins for an ESP32-S3 DevKitC-1 (avoids USB 19/20, PSRAM 35–37, strapping 0/3/45/46).
struct ButtonPin {
  int pin;
  LedRole led;
};
constexpr ButtonPin kButtons[] = {
    {4, LedRole::Starter},  {5, LedRole::PitLimiter}, {6, LedRole::Generic},  {7, LedRole::Generic},
    {15, LedRole::Generic}, {16, LedRole::Generic},   {17, LedRole::Generic}, {18, LedRole::Generic},
    {8, LedRole::Generic},  {9, LedRole::Generic},    {10, LedRole::Generic}, {11, LedRole::Flag},
};
constexpr int kToggles[] = {12, 13};          // ignition, lights: one press per flip
constexpr LedRole kToggleLeds[] = {LedRole::Ignition, LedRole::Generic};
constexpr int kEncoders[][2] = {{1, 2}, {14, 21}, {38, 39}, {40, 41}};  // ABS, TC, MAP, BB
constexpr int kPinLeds = 42;

constexpr size_t kNumButtons = sizeof(kButtons) / sizeof(kButtons[0]);
constexpr size_t kNumToggles = sizeof(kToggles) / sizeof(kToggles[0]);
constexpr size_t kNumEncoders = sizeof(kEncoders) / sizeof(kEncoders[0]);
constexpr size_t kNumLeds = kNumButtons + kNumToggles;
// Gamepad button numbers: buttons 1–12, toggles 13–14, encoders from 17
// (CW, CCW pairs) — so encoders keep their numbers if buttons are added.
constexpr uint8_t kToggleBase = 12, kEncoderBase = 16;
constexpr uint8_t kLedBrightness = 80;
constexpr uint16_t kLedMaxMilliamps = 200;

USBHIDGamepad gamepad;
FrameParser parser;
State state;
ScreenManager telemetryLink;  // only used for its telemetry timeout
Debouncer buttons[kNumButtons];
Toggle toggles[kNumToggles];
Encoder encoders[kNumEncoders];
Adafruit_NeoPixel leds(kNumLeds, kPinLeds, NEO_GRB + NEO_KHZ800);

}  // namespace

void setup() {
  Serial.begin(115200);  // telemetry from the bridge (CDC)
  gamepad.begin();
  USB.begin();
  for (const auto& b : kButtons) pinMode(b.pin, INPUT_PULLUP);
  for (int p : kToggles) pinMode(p, INPUT_PULLUP);
  for (const auto& e : kEncoders) {
    pinMode(e[0], INPUT_PULLUP);
    pinMode(e[1], INPUT_PULLUP);
  }
  leds.begin();
  leds.show();
}

void loop() {
  const uint32_t now = millis();
  while (Serial.available() > 0) {
    parser.push(static_cast<uint8_t>(Serial.read()));
    while (parser.next()) {
      if (applyFrame(parser.frame(), state) == kMsgTelemetry) telemetryLink.onTelemetry(state.telemetry, now);
    }
  }

  uint32_t report = 0;
  for (size_t i = 0; i < kNumButtons; ++i) {
    if (buttons[i].update(digitalRead(kButtons[i].pin) == LOW, now)) report |= 1u << i;
  }
  for (size_t i = 0; i < kNumToggles; ++i) {
    toggles[i].update(digitalRead(kToggles[i]) == LOW, now);
    if (toggles[i].pressed(now)) report |= 1u << (kToggleBase + i);
  }
  for (size_t i = 0; i < kNumEncoders; ++i) {
    encoders[i].update(digitalRead(kEncoders[i][0]) == LOW, digitalRead(kEncoders[i][1]) == LOW, now);
    if (encoders[i].cwPressed(now)) report |= 1u << (kEncoderBase + 2 * i);
    if (encoders[i].ccwPressed(now)) report |= 1u << (kEncoderBase + 2 * i + 1);
  }
  static uint32_t lastReport = 0xFFFFFFFF;
  if (report != lastReport) {
    gamepad.send(0, 0, 0, 0, 0, 0, 0, report);
    lastReport = report;
  }

  static uint32_t lastLeds = 0;
  if (now - lastLeds >= 33) {
    lastLeds = now;
    const bool live = telemetryLink.hasSignal(now);
    Rgb px[kNumLeds];
    for (size_t i = 0; i < kNumButtons; ++i) px[i] = buttonLed(kButtons[i].led, buttons[i].pressed(), state, live, now);
    for (size_t i = 0; i < kNumToggles; ++i) {
      px[kNumButtons + i] = buttonLed(kToggleLeds[i], toggles[i].on(), state, live, now);
    }
    leds.setBrightness(limitBrightness(px, kNumLeds, kLedBrightness, kLedMaxMilliamps));
    for (size_t i = 0; i < kNumLeds; ++i) leds.setPixelColor(i, px[i].r, px[i].g, px[i].b);
    leds.show();
  }
}
