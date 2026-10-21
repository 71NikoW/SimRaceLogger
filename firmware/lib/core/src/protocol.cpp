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
int16_t i16(const uint8_t* p) { return static_cast<int16_t>(u16(p)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
int32_t i32(const uint8_t* p) { return static_cast<int32_t>(u32(p)); }
template <typename E>
E enumOr(uint8_t v, uint8_t max, E fallback) {
  return v <= max ? static_cast<E>(v) : fallback;
}
}  // namespace

bool decodeTelemetry(const uint8_t* p, size_t len, Telemetry& t) {
  if (len < kTelemetryMinSize) return false;
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
  const uint8_t flag = len > 37 ? p[37] : 0;
  t.race_flag = flag <= 9 ? static_cast<RaceFlag>(flag) : RaceFlag::None;
  return true;
}

bool decodeEngine(const uint8_t* p, size_t len, Engine& e) {
  if (len < kEngineSize) return false;
  e.rpm = u16(p);
  e.shift_rpm = u16(p + 2);
  e.max_rpm = u16(p + 4);
  return true;
}

bool decodeTyres(const uint8_t* p, size_t len, Tyres& t) {
  if (len < kTyresSize) return false;
  for (int i = 0; i < 4; ++i) {
    const uint8_t* w = p + i * 7;
    t.wheel[i].temp_x10 = i16(w);
    t.wheel[i].pressure_psi_x100 = u16(w + 2);
    t.wheel[i].wear_pct = w[4] > 100 ? 100 : w[4];
    t.wheel[i].brake_temp_c = i16(w + 5);
  }
  t.opt_min_x10 = i16(p + 28);
  t.opt_max_x10 = i16(p + 30);
  t.target_psi_x100 = u16(p + 32);
  t.compound = enumOr(p[34], 5, Compound::Unknown);
  return true;
}

bool decodeCar(const uint8_t* p, size_t len, Car& c) {
  if (len < kCarSize) return false;
  c.abs = p[0];
  c.tc = p[1];
  c.tc_cut = p[2];
  c.engine_map = p[3];
  c.brake_bias_x10 = u16(p + 4);
  c.damage_pct = p[6] > 100 ? 100 : p[6];
  return true;
}

bool decodeFuel(const uint8_t* p, size_t len, Fuel& f) {
  if (len < kFuelSize) return false;
  f.liters_x10 = u16(p);
  f.per_lap_x100 = u16(p + 2);
  f.capacity_x10 = u16(p + 4);
  f.to_finish_x10 = i16(p + 6);
  return true;
}

bool decodeRace(const uint8_t* p, size_t len, Race& r) {
  if (len < kRaceSize) return false;
  r.gap_ahead_ms = i32(p);
  r.gap_behind_ms = i32(p + 4);
  r.time_left_s = u32(p + 8);
  r.laps_left = u16(p + 12);
  r.car_count = p[14];
  r.penalty = enumOr(p[15], 4, Penalty::None);
  r.penalty_s = p[16];
  r.track_limits = p[17];
  r.track_limits_max = p[18];
  r.pit_window = enumOr(p[19], 2, PitWindow::Unknown);
  r.bits = p[20] & 0x03;
  return true;
}

bool decodeTiming(const uint8_t* p, size_t len, Timing& t) {
  if (len < kTimingSize) return false;
  t.sector = p[0] > 2 ? 0 : p[0];
  for (int i = 0; i < 3; ++i) {
    t.sector_ms[i] = u32(p + 1 + i * 4);
    t.best_sector_ms[i] = u32(p + 13 + i * 4);
    t.state[i] = enumOr(p[25 + i], 3, SectorState::None);
  }
  t.optimal_lap_ms = u32(p + 28);
  t.session_best_ms = u32(p + 32);
  return true;
}

bool decodeWeather(const uint8_t* p, size_t len, Weather& w) {
  if (len < kWeatherSize) return false;
  auto lvl = [](uint8_t v) -> uint8_t { return v > 5 ? 5 : v; };
  w.rain = lvl(p[0]);
  w.rain_in_10 = lvl(p[1]);
  w.rain_in_30 = lvl(p[2]);
  w.wetness = lvl(p[3]);
  w.grip_pct = p[4] > 100 ? 100 : p[4];
  w.wind_kmh = p[5];
  w.wind_dir_deg = u16(p + 6) % 360;
  w.clouds_pct = p[8] > 100 ? 100 : p[8];
  return true;
}

bool decodeLapHistory(const uint8_t* p, size_t len, LapHistory& h) {
  if (len < kLapHistorySize) return false;
  h.count = p[0] > 5 ? 5 : p[0];
  for (int i = 0; i < 5; ++i) h.lap_ms[i] = i < h.count ? u32(p + 1 + i * 4) : 0;
  h.valid_mask = p[21] & ((1u << h.count) - 1);
  return true;
}

bool decodeConfig(const uint8_t* p, size_t len, Config& c) {
  if (len < kConfigSize) return false;
  c.page_mask = u16(p);
  c.brightness = p[2];
  c.units = p[3] & 0x07;
  return true;
}

uint8_t applyFrame(const Frame& f, State& s) {
  bool ok = false;
  switch (f.type) {
    case kMsgTelemetry: ok = decodeTelemetry(f.payload, f.len, s.telemetry); break;
    case kMsgEngine: s.has_engine |= ok = decodeEngine(f.payload, f.len, s.engine); break;
    case kMsgTyres: s.has_tyres |= ok = decodeTyres(f.payload, f.len, s.tyres); break;
    case kMsgCar: s.has_car |= ok = decodeCar(f.payload, f.len, s.car); break;
    case kMsgFuel: s.has_fuel |= ok = decodeFuel(f.payload, f.len, s.fuel); break;
    case kMsgRace: s.has_race |= ok = decodeRace(f.payload, f.len, s.race); break;
    case kMsgTiming: s.has_timing |= ok = decodeTiming(f.payload, f.len, s.timing); break;
    case kMsgWeather: s.has_weather |= ok = decodeWeather(f.payload, f.len, s.weather); break;
    case kMsgLapHistory: s.has_history |= ok = decodeLapHistory(f.payload, f.len, s.history); break;
    case kMsgConfig: ok = decodeConfig(f.payload, f.len, s.config); break;
    case kMsgTrackInfo: {
      const size_t n = f.len < kMaxTrackName ? f.len : kMaxTrackName;
      memcpy(s.track_name, f.payload, n);
      s.track_name[n] = '\0';
      ok = true;
      break;
    }
    case kMsgHello: ok = f.len >= 1; break;
    default: break;
  }
  return ok ? f.type : 0;
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
