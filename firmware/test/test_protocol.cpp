#include <vector>

#include "check.h"
#include "rigdash/protocol.h"

using namespace rigdash;

static std::vector<uint8_t> makeFrame(uint8_t type, const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> body{type, static_cast<uint8_t>(payload.size())};
  body.insert(body.end(), payload.begin(), payload.end());
  const uint16_t crc = crc16(body.data(), body.size());
  std::vector<uint8_t> f{kSof};
  f.insert(f.end(), body.begin(), body.end());
  f.push_back(crc & 0xFF);
  f.push_back(crc >> 8);
  return f;
}

static int feed(FrameParser& p, const std::vector<uint8_t>& bytes, std::vector<Frame>* out = nullptr) {
  int n = 0;
  for (uint8_t b : bytes) {
    p.push(b);
    while (p.next()) {
      ++n;
      if (out) out->push_back(p.frame());
    }
  }
  return n;
}

TEST(crc16_check_value) {
  const char* s = "123456789";
  CHECK_EQ(crc16(reinterpret_cast<const uint8_t*>(s), 9), 0x29B1);
}

TEST(parser_accepts_valid_frame) {
  FrameParser p;
  std::vector<Frame> frames;
  CHECK_EQ(feed(p, makeFrame(0x01, {0x01}), &frames), 1);
  CHECK_EQ(frames[0].type, 0x01);
  CHECK_EQ(frames[0].len, 1);
  CHECK_EQ(frames[0].payload[0], 0x01);
}

TEST(parser_empty_payload) {
  FrameParser p;
  CHECK_EQ(feed(p, makeFrame(0x42, {})), 1);
  CHECK_EQ(p.frame().len, 0);
}

TEST(parser_skips_leading_garbage) {
  FrameParser p;
  std::vector<uint8_t> s{0x00, 0x13, 0xFF, 0xA5, 0x99};
  auto f = makeFrame(0x11, {'S', 'p', 'a'});
  s.insert(s.end(), f.begin(), f.end());
  std::vector<Frame> frames;
  CHECK_EQ(feed(p, s, &frames), 1);
  CHECK_EQ(frames[0].type, 0x11);
}

TEST(parser_rejects_bad_crc_and_recovers) {
  FrameParser p;
  auto bad = makeFrame(0x10, std::vector<uint8_t>(kTelemetrySize, 0x55));
  bad[8] ^= 0x01;
  auto good = makeFrame(0x01, {0x01});
  bad.insert(bad.end(), good.begin(), good.end());
  std::vector<Frame> frames;
  CHECK_EQ(feed(p, bad, &frames), 1);
  CHECK_EQ(frames[0].type, 0x01);
  CHECK(p.crcErrors() >= 1);
}

TEST(parser_recovers_from_truncated_frame_followed_by_valid_one) {
  // Frame cut off mid-payload (e.g. bridge restarted), then a good frame.
  FrameParser p;
  auto cut = makeFrame(0x10, std::vector<uint8_t>(kTelemetrySize, 0x00));
  cut.resize(10);
  auto good = makeFrame(0x01, {0x01});
  std::vector<uint8_t> s = cut;
  for (int i = 0; i < 3; ++i) s.insert(s.end(), good.begin(), good.end());
  // Parser can't know the cut frame is dead until 42 bytes arrived.
  CHECK_EQ(feed(p, s), 0);
  // More traffic exposes the bad CRC; all buffered good frames are recovered.
  CHECK_EQ(feed(p, std::vector<uint8_t>(good.begin(), good.end())) +
               feed(p, std::vector<uint8_t>(good.begin(), good.end())) +
               feed(p, std::vector<uint8_t>(good.begin(), good.end())) +
               feed(p, std::vector<uint8_t>(good.begin(), good.end())),
           7);
  CHECK_EQ(p.crcErrors(), 1u);
}

TEST(parser_rejects_oversize_length) {
  FrameParser p;
  std::vector<uint8_t> s{kSof, 0x10, 200};
  auto good = makeFrame(0x01, {0x01});
  s.insert(s.end(), good.begin(), good.end());
  CHECK_EQ(feed(p, s), 1);
}

TEST(parser_back_to_back_frames) {
  FrameParser p;
  std::vector<uint8_t> s;
  for (int i = 0; i < 50; ++i) {
    auto f = makeFrame(0x11, {static_cast<uint8_t>(i), kSof, 0x00});
    s.insert(s.end(), f.begin(), f.end());
  }
  CHECK_EQ(feed(p, s), 50);
  CHECK_EQ(p.crcErrors(), 0u);
}

TEST(decode_telemetry_too_short) {
  uint8_t buf[kTelemetryMinSize - 1] = {};
  Telemetry t;
  CHECK(!decodeTelemetry(buf, sizeof buf, t));
}

TEST(decode_telemetry_accepts_longer_payload) {
  uint8_t buf[kTelemetrySize + 4] = {};
  buf[36] = 3;
  Telemetry t;
  CHECK(decodeTelemetry(buf, sizeof buf, t));
  CHECK(t.session == Session::Race);
}

TEST(decode_telemetry_old_37_byte_payload) {
  uint8_t buf[kTelemetryMinSize] = {};
  buf[20] = 7;
  Telemetry t;
  t.race_flag = RaceFlag::Red;
  CHECK(decodeTelemetry(buf, sizeof buf, t));
  CHECK_EQ(t.lap_number, 7);
  CHECK(t.race_flag == RaceFlag::None);
}

TEST(decode_telemetry_race_flag) {
  uint8_t buf[kTelemetrySize] = {};
  Telemetry t;
  buf[37] = 3;
  CHECK(decodeTelemetry(buf, sizeof buf, t));
  CHECK(t.race_flag == RaceFlag::Blue);
  buf[37] = 9;
  CHECK(decodeTelemetry(buf, sizeof buf, t));
  CHECK(t.race_flag == RaceFlag::VirtualSafetyCar);
  buf[37] = 99;
  CHECK(decodeTelemetry(buf, sizeof buf, t));
  CHECK(t.race_flag == RaceFlag::None);
}

TEST(decode_telemetry_unknown_session_and_reserved_flags) {
  uint8_t buf[kTelemetrySize] = {};
  buf[34] = 0xFF;
  buf[35] = 0xFF;
  buf[36] = 77;
  Telemetry t;
  CHECK(decodeTelemetry(buf, sizeof buf, t));
  CHECK_EQ(t.flags, 0x1F);
  CHECK(t.session == Session::Unknown);
}

TEST(parser_survives_random_noise) {
  // Noise between valid frames must never crash and must not hide later frames.
  FrameParser p;
  uint32_t seed = 12345;
  auto rnd = [&seed]() { return seed = seed * 1103515245u + 12345u, static_cast<uint8_t>(seed >> 16); };
  auto good = makeFrame(0x01, {0x01});
  int got = 0;
  for (int round = 0; round < 500; ++round) {
    std::vector<uint8_t> noise(rnd() % 80);
    for (auto& b : noise) b = rnd();
    feed(p, noise);
    got += feed(p, good);
  }
  // Noise may swallow a frame now and then (e.g. a fake SOF+len), never most.
  CHECK(got > 400);
}
