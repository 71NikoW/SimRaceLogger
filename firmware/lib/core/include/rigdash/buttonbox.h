#pragma once
// Button box: buttons, toggle switches and rotary encoders become one USB
// gamepad report; LEDs behind the buttons react to telemetry. Pure logic.
#include <stdint.h>

#include "rigdash/dash.h"
#include "rigdash/telemetry.h"

namespace rigdash {

constexpr uint32_t kButtonDebounceMs = 5;
constexpr uint32_t kPulseMs = 60;      // encoder step / toggle flip = short press
constexpr uint8_t kMaxEncoderQueue = 8;

class Debouncer {
 public:
  // raw: true = pressed. Returns the stable state.
  bool update(bool raw, uint32_t now_ms);
  bool pressed() const { return stable_; }

 private:
  bool stable_ = false;
  bool last_raw_ = false;
  uint32_t changed_ms_ = 0;
};

// Full-step quadrature decoder (Gray code) with a pulse queue: every detent
// becomes one short "press" of the CW or CCW button, which is how sims expect
// encoders to work.
class Encoder {
 public:
  void update(bool a, bool b, uint32_t now_ms);
  bool cwPressed(uint32_t now_ms) const { return active_ > 0 && now_ms - since_ < kPulseMs; }
  bool ccwPressed(uint32_t now_ms) const { return active_ < 0 && now_ms - since_ < kPulseMs; }
  int8_t pending() const { return queued_; }

 private:
  void next(uint32_t now_ms);
  uint8_t state_ = 0;
  bool init_ = false;
  int8_t sub_ = 0;      // quarter steps within one detent
  int8_t queued_ = 0;   // + CW, - CCW
  int8_t active_ = 0;
  uint32_t since_ = 0;
};

// A toggle switch reported as a short press on every flip (what most sims
// want for ignition / lights), or held while on.
class Toggle {
 public:
  explicit Toggle(bool momentary = true) : momentary_(momentary) {}
  void update(bool on, uint32_t now_ms);
  bool pressed(uint32_t now_ms) const;
  bool on() const { return on_; }

 private:
  bool momentary_;
  bool on_ = false;
  bool init_ = false;
  uint32_t flipped_ms_ = 0;
};

// What a button's LED shows.
enum class LedRole : uint8_t {
  None,
  Generic,      // dim white backlight, bright while pressed
  PitLimiter,   // blinks green while the limiter is on
  Ignition,     // red with ignition on and engine off, green when running
  Starter,      // blinks red when ignition is on but the engine is off
  Flag,         // shows the race flag colour
};

Rgb buttonLed(LedRole role, bool pressed, const State& s, bool has_signal, uint32_t now_ms);

}  // namespace rigdash
