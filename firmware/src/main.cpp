// Board layer: USB serial -> FrameParser -> ScreenManager -> renderer.
// The renderer is a serial text stub until the display driver is chosen.
#include <Arduino.h>

#include "rigdash/format.h"
#include "rigdash/protocol.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

namespace {

constexpr int kPinButtonPage = 4;
constexpr uint32_t kRenderIntervalMs = 33;  // ~30 fps
constexpr uint32_t kDebounceMs = 30;

FrameParser parser;
ScreenManager screens;
Telemetry telemetry;
char trackName[kMaxTrackName + 1] = "";

const char* screenName(Screen s) {
  switch (s) {
    case Screen::NoSignal: return "WAITING FOR TELEMETRY";
    case Screen::LapInvalidated: return "LAP INVALIDATED";
    case Screen::PitLimiter: return "PIT LIMITER ON";
    case Screen::EngineOff: return "ENGINE OFF";
    case Screen::CurrentLap: return "CURRENT LAP";
    case Screen::TrackData: return "TRACK DATA";
    case Screen::LapSummary: return "LAP SUMMARY";
  }
  return "?";
}

void handleFrame(const Frame& f, uint32_t now) {
  switch (f.type) {
    case kMsgTelemetry:
      if (decodeTelemetry(f.payload, f.len, telemetry)) screens.onTelemetry(telemetry, now);
      break;
    case kMsgTrackInfo: {
      const size_t n = f.len < kMaxTrackName ? f.len : kMaxTrackName;
      memcpy(trackName, f.payload, n);
      trackName[n] = '\0';
      break;
    }
    default:
      break;  // HELLO and unknown types: nothing to do yet
  }
}

// TODO(prototype): replace with LovyanGFX/LVGL drawing per screen.
void render(Screen s) {
  static Screen lastScreen = Screen::NoSignal;
  static uint32_t lastLap = 0xFFFFFFFF;
  const uint32_t shown = telemetry.current_lap_ms / 100;
  if (s == lastScreen && shown == lastLap) return;
  lastScreen = s;
  lastLap = shown;
  char lap[16], fuel[16];
  formatLapTime(telemetry.current_lap_ms, lap, sizeof lap);
  formatFuelLaps(telemetry.fuel_laps_x10, fuel, sizeof fuel);
  Serial0.printf("[%s] %s  LAP %02u  POS %u  %s  %s\n", screenName(s), lap,
                 telemetry.lap_number, telemetry.position, fuel, trackName);
}

}  // namespace

void setup() {
  Serial.begin(115200);   // USB CDC: telemetry in
  Serial0.begin(115200);  // UART0: debug out
  pinMode(kPinButtonPage, INPUT_PULLUP);
}

void loop() {
  const uint32_t now = millis();
  while (Serial.available() > 0) {
    parser.push(static_cast<uint8_t>(Serial.read()));
    while (parser.next()) handleFrame(parser.frame(), now);
  }

  static bool lastPressed = false;
  static uint32_t lastEdge = 0;
  const bool pressed = digitalRead(kPinButtonPage) == LOW;
  if (pressed != lastPressed && now - lastEdge > kDebounceMs) {
    lastEdge = now;
    lastPressed = pressed;
    if (pressed) screens.nextPage();
  }

  static uint32_t lastRender = 0;
  if (now - lastRender >= kRenderIntervalMs) {
    lastRender = now;
    render(screens.current(now));
  }
}
