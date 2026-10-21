// Board layer: USB serial -> FrameParser -> State -> AlertEngine/ScreenManager
// -> renderer + LEDs. The renderer is a serial text stub until the display
// driver is chosen; LEDs and buttons are real.
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "rigdash/alerts.h"
#include "rigdash/dash.h"
#include "rigdash/format.h"
#include "rigdash/protocol.h"
#include "rigdash/screen_manager.h"

using namespace rigdash;

namespace {

constexpr int kPinButtonPage = 4;     // short press: next page, long press: previous
constexpr int kPinButtonBright = 5;   // cycles brightness
constexpr int kPinBacklight = 7;
// Two status LEDs, common cathode RGB (R, G, B per LED).
constexpr int kPinLed[2][3] = {{15, 16, 17}, {18, 8, 3}};
// LED frame: one WS2812B / SK6812 chain — 12 on top, 4 down each side.
constexpr int kPinStrip = 21;
constexpr StripLayout kStripLayout{12, 4};
// USB 2.0 gives 500 mA. 20 LEDs at full white draw ~1.2 A, so cap the strip.
constexpr uint8_t kStripMaxBrightness = 60;
constexpr uint32_t kRenderIntervalMs = 33;  // ~30 fps
constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kLongPressMs = 600;

FrameParser parser;
State state;
ScreenManager screens;
AlertEngine alerts;
uint8_t brightness = 200;
Adafruit_NeoPixel strip(kStripLayout.count(), kPinStrip, NEO_GRB + NEO_KHZ800);
bool brightnessFromBridge = true;

const char* screenName(Screen s) {
  switch (s) {
    case Screen::NoSignal: return "WAITING FOR TELEMETRY";
    case Screen::RaceFlag: return "RACE FLAG";
    case Screen::LapInvalidated: return "LAP INVALIDATED";
    case Screen::Alert: return "ALERT";
    case Screen::PitLimiter: return "PIT LIMITER";
    case Screen::EngineOff: return "ENGINE OFF";
    case Screen::CurrentLap: return "CURRENT LAP";
    case Screen::Delta: return "DELTA";
    case Screen::Tyres: return "TYRES";
    case Screen::Car: return "CAR";
    case Screen::Race: return "RACE";
    case Screen::Weather: return "WEATHER";
    case Screen::TrackData: return "TRACK DATA";
    case Screen::Laps: return "LAPS";
  }
  return "?";
}

void handleFrame(const Frame& f, uint32_t now) {
  const uint8_t type = applyFrame(f, state);
  if (type == kMsgTelemetry) screens.onTelemetry(state.telemetry, now);
  if (type == kMsgConfig) {
    screens.setPageMask(state.config.page_mask);
    if (brightnessFromBridge) brightness = state.config.brightness;
  }
  if (type != 0) alerts.update(state, now);
}

void writeLed(int idx, Led c) {
  // R, G, B on/off per colour.
  static const uint8_t kRgb[][3] = {
      {0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}, {1, 1, 1}, {1, 0, 1}};
  const uint8_t* rgb = kRgb[static_cast<uint8_t>(c)];
  for (int i = 0; i < 3; ++i) digitalWrite(kPinLed[idx][i], rgb[i] ? HIGH : LOW);
}

// TODO(prototype): replace with LovyanGFX/LVGL drawing per screen; the
// simulator (simulator/index.html) is the reference layout.
void render(Screen s, uint32_t now) {
  static Screen lastScreen = Screen::NoSignal;
  static uint32_t lastTenth = 0xFFFFFFFF;
  const uint32_t tenth = state.telemetry.current_lap_ms / 100;
  if (s == lastScreen && tenth == lastTenth) return;
  lastScreen = s;
  lastTenth = tenth;
  char lap[16], fuel[16];
  formatLapTime(state.telemetry.current_lap_ms, lap, sizeof lap);
  formatFuelLaps(state.telemetry.fuel_laps_x10, fuel, sizeof fuel);
  Serial0.printf("[%s] %s  LAP %02u  POS %u  %s  %s  %s", screenName(s), lap,
                 state.telemetry.lap_number, state.telemetry.position, fuel,
                 raceFlagText(screens.raceFlag()), state.track_name);
  if (s == Screen::Alert) Serial0.printf("  alert=%u", static_cast<unsigned>(alerts.current(now).kind));
  Serial0.println();
}

}  // namespace

void setup() {
  Serial.begin(115200);   // USB CDC: telemetry in
  Serial0.begin(115200);  // UART0: debug out
  pinMode(kPinButtonPage, INPUT_PULLUP);
  pinMode(kPinButtonBright, INPUT_PULLUP);
  for (auto& led : kPinLed) {
    for (int pin : led) pinMode(pin, OUTPUT);
  }
  ledcAttach(kPinBacklight, 5000, 8);
  strip.begin();
  strip.show();
}

namespace {
void writeStrip(Screen s, uint32_t now) {
  Rgb px[kStripLayout.count()];
  stripFor(s, state, alerts.current(now).kind, now, kStripLayout, px);
  // Follow the display brightness, capped for USB power.
  strip.setBrightness(static_cast<uint8_t>(brightness * kStripMaxBrightness / 255));
  for (uint16_t i = 0; i < kStripLayout.count(); ++i) strip.setPixelColor(i, px[i].r, px[i].g, px[i].b);
  strip.show();
}
}  // namespace

void loop() {
  const uint32_t now = millis();
  while (Serial.available() > 0) {
    parser.push(static_cast<uint8_t>(Serial.read()));
    while (parser.next()) handleFrame(parser.frame(), now);
  }

  // Page button: short press next, long press previous.
  static bool pagePressed = false;
  static uint32_t pageEdge = 0;
  const bool p = digitalRead(kPinButtonPage) == LOW;
  if (p != pagePressed && now - pageEdge > kDebounceMs) {
    if (!p) (now - pageEdge >= kLongPressMs) ? screens.prevPage() : screens.nextPage();
    pagePressed = p;
    pageEdge = now;
  }
  // Brightness button overrides the bridge value until reboot.
  static bool brightPressed = false;
  static uint32_t brightEdge = 0;
  const bool b = digitalRead(kPinButtonBright) == LOW;
  if (b != brightPressed && now - brightEdge > kDebounceMs) {
    if (b) {
      static const uint8_t kSteps[] = {40, 100, 170, 255};
      uint8_t next = kSteps[0];
      for (uint8_t step : kSteps) {
        if (step > brightness) {
          next = step;
          break;
        }
      }
      brightness = next;
      brightnessFromBridge = false;
    }
    brightPressed = b;
    brightEdge = now;
  }

  static uint32_t lastRender = 0;
  if (now - lastRender >= kRenderIntervalMs) {
    lastRender = now;
    alerts.update(state, now);  // expires popups even without new data
    const Screen s = screens.current(now, alerts.active(now));
    render(s, now);
    const Leds leds = ledsFor(s, state, now);
    writeLed(0, leds.left);
    writeLed(1, leds.right);
    writeStrip(s, now);
    ledcWrite(kPinBacklight, brightness);
  }
}
