#include "rigdash/buttonbox.h"

#include "rigdash/screen_manager.h"

namespace rigdash {

bool Debouncer::update(bool raw, uint32_t now) {
  if (raw != last_raw_) {
    last_raw_ = raw;
    changed_ms_ = now;
  } else if (raw != stable_ && now - changed_ms_ >= kButtonDebounceMs) {
    stable_ = raw;
  }
  return stable_;
}

void Encoder::next(uint32_t now) {
  if (active_ != 0 && now - since_ < 2 * kPulseMs) return;  // press + release gap
  active_ = 0;
  if (queued_ == 0) return;
  active_ = queued_ > 0 ? 1 : -1;
  queued_ = static_cast<int8_t>(queued_ - active_);
  since_ = now;
}

void Encoder::update(bool a, bool b, uint32_t now) {
  // Gray sequence 00 → 01 → 11 → 10 → 00 = clockwise.
  const uint8_t s = static_cast<uint8_t>((a ? 2 : 0) | (b ? 1 : 0));
  static const int8_t kStep[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
  if (!init_) {
    state_ = s;
    init_ = true;
  } else if (s != state_) {
    sub_ = static_cast<int8_t>(sub_ + kStep[(state_ << 2) | s]);
    state_ = s;
    // One detent = 4 quarter steps, counted when the encoder rests at 00/11.
    if (sub_ >= 4 || sub_ <= -4) {
      const int8_t dir = sub_ > 0 ? 1 : -1;
      if ((dir > 0 && queued_ < kMaxEncoderQueue) || (dir < 0 && queued_ > -kMaxEncoderQueue)) {
        queued_ = static_cast<int8_t>(queued_ + dir);
      }
      sub_ = 0;
    }
  }
  next(now);
}

void Toggle::update(bool on, uint32_t now) {
  if (!init_) {
    on_ = on;
    init_ = true;
    flipped_ms_ = now - kPulseMs;  // no press for the position at power-up
    return;
  }
  if (on != on_) {
    on_ = on;
    flipped_ms_ = now;
  }
}

bool Toggle::pressed(uint32_t now) const {
  if (!momentary_) return on_;
  return init_ && now - flipped_ms_ < kPulseMs;
}

namespace {
constexpr Rgb kBacklight{18, 18, 18};
constexpr Rgb kPressed{180, 180, 180};
}  // namespace

Rgb buttonLed(LedRole role, bool pressed, const State& s, bool has_signal, uint32_t now) {
  const Telemetry& t = s.telemetry;
  const bool blink = blinkOn(now);
  switch (role) {
    case LedRole::None:
      return rgb::kOff;
    case LedRole::Generic:
      return pressed ? kPressed : kBacklight;
    case LedRole::PitLimiter:
      if (has_signal && t.has(kFlagPitLimiter)) return blink ? rgb::kGreen : rgb::kOff;
      return pressed ? kPressed : kBacklight;
    case LedRole::Ignition:
      if (!has_signal) return pressed ? kPressed : kBacklight;
      if (t.has(kFlagEngineOn)) return rgb::kGreen;
      if (t.has(kFlagIgnitionOn)) return rgb::kRed;
      return kBacklight;
    case LedRole::Starter:
      if (has_signal && t.has(kFlagIgnitionOn) && !t.has(kFlagEngineOn)) {
        return blink ? rgb::kRed : rgb::kOff;
      }
      return pressed ? kPressed : kBacklight;
    case LedRole::Flag: {
      if (!has_signal) return kBacklight;
      switch (t.race_flag) {
        case RaceFlag::Yellow:
        case RaceFlag::SafetyCar:
        case RaceFlag::VirtualSafetyCar: return blink ? rgb::kYellow : rgb::kOff;
        case RaceFlag::Blue: return blink ? rgb::kBlue : rgb::kOff;
        case RaceFlag::Red:
        case RaceFlag::Black: return rgb::kRed;
        case RaceFlag::White:
        case RaceFlag::Chequered: return rgb::kWhite;
        case RaceFlag::Green: return rgb::kDimGreen;
        default: return kBacklight;
      }
    }
  }
  return rgb::kOff;
}

}  // namespace rigdash
