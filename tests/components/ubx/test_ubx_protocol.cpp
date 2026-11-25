#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "esphome/components/ubx/ubx_protocol.h"
#include "esphome/core/helpers.h"

namespace esphome::ubx::testing {

static std::vector<uint8_t> frame(uint8_t cls, uint8_t id, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> out(payload.size() + UBX_FRAME_OVERHEAD);
  out.resize(build_frame(cls, id, payload.data(), payload.size(), out.data()));
  return out;
}

static int feed_all(FrameParser &parser, const std::vector<uint8_t> &bytes) {
  int frames = 0;
  for (uint8_t b : bytes)
    frames += parser.feed(b) ? 1 : 0;
  return frames;
}

TEST(UBXFrame, MonVerPollMatchesTheReceiverDescription) {
  EXPECT_EQ(frame(UBX_CLASS_MON, UBX_MON_VER, {}),
            (std::vector<uint8_t>{0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34}));
}

TEST(UBXFrame, ParserSkipsNmeaAndFindsTheFrame) {
  std::vector<uint8_t> bytes{'$', 'G', 'P', 'R', 'M', 'C', ',', 0xB5, '\r', '\n'};
  auto f = frame(UBX_CLASS_ACK, UBX_ACK_ACK, {UBX_CLASS_CFG, UBX_CFG_MSG});
  bytes.insert(bytes.end(), f.begin(), f.end());
  FrameParser parser;
  EXPECT_EQ(feed_all(parser, bytes), 1);
  EXPECT_EQ(parser.msg_class(), UBX_CLASS_ACK);
  EXPECT_EQ(parser.msg_id(), UBX_ACK_ACK);
  ASSERT_EQ(parser.length(), 2);
  EXPECT_EQ(parser.payload()[1], UBX_CFG_MSG);
}

TEST(UBXFrame, ParserRejectsABadChecksum) {
  auto f = frame(UBX_CLASS_ACK, UBX_ACK_ACK, {UBX_CLASS_CFG, UBX_CFG_MSG});
  f.back() ^= 0xFF;
  FrameParser parser;
  EXPECT_EQ(feed_all(parser, f), 0);
  // and recovers for the next one
  EXPECT_EQ(feed_all(parser, frame(UBX_CLASS_ACK, UBX_ACK_NAK, {UBX_CLASS_CFG, UBX_CFG_NAV5})), 1);
}

static std::vector<uint8_t> mon_ver(const char *sw, const std::vector<const char *> &extensions) {
  std::vector<uint8_t> p(40 + 30 * extensions.size(), 0);
  strncpy(reinterpret_cast<char *>(p.data()), sw, 30);
  strncpy(reinterpret_cast<char *>(p.data() + 30), "00040007", 10);
  for (size_t i = 0; i < extensions.size(); i++)
    strncpy(reinterpret_cast<char *>(p.data() + 40 + 30 * i), extensions[i], 30);
  return p;
}

TEST(UBXMonVer, ProtocolVersion) {
  auto u6 = mon_ver("7.03 (45969)", {});
  EXPECT_EQ(parse_protocol_version(u6.data(), u6.size()), 0);
  auto u7 = mon_ver("1.00 (59842)", {"PROTVER 14.00", "GPS;SBAS;GLO;QZSS"});
  EXPECT_EQ(parse_protocol_version(u7.data(), u7.size()), 1400);
  auto m8 = mon_ver("ROM CORE 3.01 (107888)", {"FWVER=SPG 3.01", "PROTVER=18.00", "GPS;GLO;GAL;BDS"});
  EXPECT_EQ(parse_protocol_version(m8.data(), m8.size()), 1800);
}

TEST(UBXNav, PvtWithA3dFix) {
  std::vector<uint8_t> p(92, 0);
  p[20] = 3;     // 3D fix
  p[21] = 0x01;  // gnssFixOK
  p[23] = 9;
  put_le32(p.data() + 24, static_cast<uint32_t>(173657050));  // lon 17.365705
  put_le32(p.data() + 28, static_cast<uint32_t>(480537950));  // lat 48.053795
  put_le32(p.data() + 36, 122000);                            // hMSL 122 m
  put_le32(p.data() + 40, 2500);                              // hAcc 2.5 m
  put_le32(p.data() + 44, 4000);                              // vAcc 4 m
  put_le32(p.data() + 60, 10000);                             // 10 m/s
  put_le32(p.data() + 64, 9000000);                           // 90 degrees
  gnss::GNSSData d{};
  bool fix = false;
  ASSERT_TRUE(decode_nav(UBX_NAV_PVT, p.data(), p.size(), 0, d, fix));
  EXPECT_TRUE(fix);
  EXPECT_EQ(d.satellites, 9);
  EXPECT_NEAR(d.latitude, 48.053795, 1e-7);
  EXPECT_NEAR(d.longitude, 17.365705, 1e-7);
  EXPECT_FLOAT_EQ(d.altitude, 122.0f);
  EXPECT_FLOAT_EQ(d.horizontal_accuracy, 2.5f);
  EXPECT_FLOAT_EQ(d.vertical_accuracy, 4.0f);
  EXPECT_FLOAT_EQ(d.speed, 36.0f);
  EXPECT_FLOAT_EQ(d.course, 90.0f);
}

TEST(UBXNav, LegacySolCarriesTheFix) {
  std::vector<uint8_t> p(52, 0);
  p[10] = 2;  // 2D fix, but gpsFixOK not set
  p[47] = 4;
  gnss::GNSSData d{};
  bool fix = true;
  ASSERT_TRUE(decode_nav(UBX_NAV_SOL, p.data(), p.size(), 0, d, fix));
  EXPECT_FALSE(fix);
  EXPECT_EQ(d.satellites, 4);
}

TEST(UBXNav, TimeUtcOnlyWithKnownLeapSeconds) {
  std::vector<uint8_t> p(20, 0);
  put_le16(p.data() + 12, 2026);
  p[14] = 11;
  p[15] = 25;
  p[16] = 12;
  p[19] = 0x03;  // TOW and week valid, leap seconds unknown
  gnss::GNSSData d{};
  bool fix = false;
  ASSERT_TRUE(decode_nav(UBX_NAV_TIMEUTC, p.data(), p.size(), 1234, d, fix));
  EXPECT_EQ(d.utc, 0);
  p[19] = 0x07;
  ASSERT_TRUE(decode_nav(UBX_NAV_TIMEUTC, p.data(), p.size(), 1234, d, fix));
  EXPECT_EQ(d.utc, 1795608000);
  EXPECT_EQ(d.utc_millis, 1234u);
}

TEST(UBXNav, ShortMessagesAreRejected) {
  std::vector<uint8_t> p(27, 0);
  gnss::GNSSData d{};
  bool fix = false;
  EXPECT_FALSE(decode_nav(UBX_NAV_POSLLH, p.data(), p.size(), 0, d, fix));
  EXPECT_TRUE(std::isnan(d.latitude));
}

}  // namespace esphome::ubx::testing
