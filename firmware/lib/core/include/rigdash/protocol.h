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
};

uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc = 0xFFFF);

bool decodeTelemetry(const uint8_t* payload, size_t len, Telemetry& out);

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

}  // namespace rigdash
