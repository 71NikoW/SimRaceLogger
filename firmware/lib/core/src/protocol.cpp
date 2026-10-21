#include "rigdash/protocol.h"

#include <string.h>

namespace rigdash {

uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc) {
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

namespace {
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

bool decodeTelemetry(const uint8_t* p, size_t len, Telemetry& t) {
  if (len < kTelemetrySize) return false;
  t.current_lap_ms = u32(p + 0);
  t.last_lap_ms = u32(p + 4);
  t.best_lap_ms = u32(p + 8);
  t.last_pit_ms = u32(p + 12);
  t.delta_ms = static_cast<int32_t>(u32(p + 16));
  t.lap_number = u16(p + 20);
  t.position = p[22];
  t.gear = static_cast<int8_t>(p[23]);
  t.speed_kmh = u16(p + 24);
  t.fuel_laps_x10 = u16(p + 26);
  t.track_pos = u16(p + 28);
  t.air_temp_x10 = static_cast<int16_t>(u16(p + 30));
  t.track_temp_x10 = static_cast<int16_t>(u16(p + 32));
  t.flags = u16(p + 34) & 0x1F;
  t.session = p[36] <= 4 ? static_cast<Session>(p[36]) : Session::Unknown;
  return true;
}

void FrameParser::dropFirst() {
  // Discard the current SOF and resync on the next one already in the buffer.
  size_t next = 1;
  while (next < n_ && buf_[next] != kSof) ++next;
  memmove(buf_, buf_ + next, n_ - next);
  n_ -= next;
}

bool FrameParser::next() {
  while (n_ > 0) {
    if (buf_[0] != kSof) {
      dropFirst();
      continue;
    }
    if (n_ < 3) return false;
    const uint8_t len = buf_[2];
    if (len > kMaxPayload) {
      dropFirst();
      continue;
    }
    const size_t total = 3u + len + 2u;
    if (n_ < total) return false;
    const uint16_t want = u16(buf_ + 3 + len);
    if (crc16(buf_ + 1, 2u + len) != want) {
      ++crc_errors_;
      dropFirst();
      continue;
    }
    frame_.type = buf_[1];
    frame_.len = len;
    memcpy(frame_.payload, buf_ + 3, len);
    memmove(buf_, buf_ + total, n_ - total);
    n_ -= total;
    return true;
  }
  return false;
}

void FrameParser::push(uint8_t byte) {
  // Can't fill up if the caller drains next(), but never write out of bounds.
  if (n_ == kBufSize) dropFirst();
  if (n_ == 0 && byte != kSof) return;
  buf_[n_++] = byte;
}

}  // namespace rigdash
