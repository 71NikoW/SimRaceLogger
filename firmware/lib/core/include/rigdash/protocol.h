#pragma once
// Serial protocol v1 — see docs/PROTOCOL.md. Mirrors bridge/src/rigdash/protocol.py.
#include <stddef.h>
#include <stdint.h>

#include "rigdash/telemetry.h"

namespace rigdash {

constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kSof = 0xA5;
constexpr uint8_t kMaxPayload = 64;
constexpr uint8_t kMaxTrackName = 48;
constexpr uint8_t kTelemetrySize = 38;
// v1.0 payloads ended before race_flag; still accepted.
constexpr uint8_t kTelemetryMinSize = 37;

enum MsgType : uint8_t {
  kMsgHello = 0x01,
  kMsgTelemetry = 0x10,
  kMsgTrackInfo = 0x11,
  kMsgEngine = 0x12,
  kMsgTyres = 0x13,
  kMsgCar = 0x14,
  kMsgFuel = 0x15,
  kMsgRace = 0x16,
  kMsgTiming = 0x17,
  kMsgWeather = 0x18,
  kMsgLapHistory = 0x19,
  kMsgConfig = 0x20,
};

// Minimum payload sizes. Longer payloads are accepted (fields get appended).
constexpr uint8_t kEngineSize = 6;
constexpr uint8_t kTyresSize = 35;
constexpr uint8_t kCarSize = 7;
constexpr uint8_t kFuelSize = 8;
constexpr uint8_t kRaceSize = 21;
constexpr uint8_t kTimingSize = 36;
constexpr uint8_t kWeatherSize = 9;
constexpr uint8_t kLapHistorySize = 22;
constexpr uint8_t kConfigSize = 4;

uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc = 0xFFFF);

bool decodeTelemetry(const uint8_t* payload, size_t len, Telemetry& out);
bool decodeEngine(const uint8_t* payload, size_t len, Engine& out);
bool decodeTyres(const uint8_t* payload, size_t len, Tyres& out);
bool decodeCar(const uint8_t* payload, size_t len, Car& out);
bool decodeFuel(const uint8_t* payload, size_t len, Fuel& out);
bool decodeRace(const uint8_t* payload, size_t len, Race& out);
bool decodeTiming(const uint8_t* payload, size_t len, Timing& out);
bool decodeWeather(const uint8_t* payload, size_t len, Weather& out);
bool decodeLapHistory(const uint8_t* payload, size_t len, LapHistory& out);
bool decodeConfig(const uint8_t* payload, size_t len, Config& out);

struct Frame {
  uint8_t type = 0;
  uint8_t len = 0;
  uint8_t payload[kMaxPayload] = {};
};

// Byte-wise state machine. On bad CRC/length it rescans the buffered bytes
// for the next SOF so frames following garbage are not lost.
//
//   parser.push(byte);
//   while (parser.next()) handle(parser.frame());
class FrameParser {
 public:
  void push(uint8_t byte);
  // True when `frame()` holds a newly completed, CRC-valid frame. Call until
  // it returns false: one resync can uncover several buffered frames.
  bool next();
  const Frame& frame() const { return frame_; }
  uint32_t crcErrors() const { return crc_errors_; }

 private:
  static constexpr size_t kBufSize = 3 + kMaxPayload + 2;
  void dropFirst();

  uint8_t buf_[kBufSize] = {};
  size_t n_ = 0;
  Frame frame_;
  uint32_t crc_errors_ = 0;
};

// Decodes `frame` into the matching part of `state`. Returns the message type
// that was applied, or 0 if the frame was unknown or too short.
uint8_t applyFrame(const Frame& frame, State& state);

}  // namespace rigdash
