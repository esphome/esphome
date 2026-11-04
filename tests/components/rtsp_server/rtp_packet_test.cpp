#include "esphome/components/rtsp_server/rtp_packet.h"
#include "gtest/gtest.h"

#include <array>
#include <cstdint>

namespace esphome::rtsp_server {
namespace {

TEST(RtpPacket, WritesJpegHeader) {
  std::array<uint8_t, RTP_TCP_HEADER_SIZE> data{};
  ASSERT_EQ(write_interleaved_rtp_header(data.data(), 0, 26, true, 140, 0x1234, 0x01020304, 0x05060708),
            RTP_TCP_HEADER_SIZE);
  EXPECT_EQ(data[0], '$');
  EXPECT_EQ(data[1], 0);
  EXPECT_EQ(data[2], 0);
  EXPECT_EQ(data[3], 152);
  EXPECT_EQ(data[4], 0x80);
  EXPECT_EQ(data[5], 0x80 | 26);
  EXPECT_EQ(data[6], 0x12);
  EXPECT_EQ(data[7], 0x34);
  EXPECT_EQ(data[8], 1);
  EXPECT_EQ(data[11], 4);
  EXPECT_EQ(data[12], 5);
  EXPECT_EQ(data[15], 8);
}

TEST(RtpPacket, WritesH264HeaderWithoutMarker) {
  std::array<uint8_t, RTP_TCP_HEADER_SIZE> data{};
  ASSERT_EQ(write_interleaved_rtp_header(data.data(), 1, 96, false, 3, 9, 90000, 42), RTP_TCP_HEADER_SIZE);
  EXPECT_EQ(data[1], 1);
  EXPECT_EQ(data[3], 15);
  EXPECT_EQ(data[5], 96);
  EXPECT_EQ(data[7], 9);
}

TEST(RtpPacket, RejectsInvalidLengthAndPayloadType) {
  std::array<uint8_t, RTP_TCP_HEADER_SIZE> data{};
  EXPECT_EQ(write_interleaved_rtp_header(data.data(), 0, 96, false, 65524, 0, 0, 0), 0);
  EXPECT_EQ(write_interleaved_rtp_header(data.data(), 0, 128, false, 0, 0, 0, 0), 0);
  EXPECT_EQ(write_interleaved_rtp_header(nullptr, 0, 96, false, 0, 0, 0, 0), 0);
}

}  // namespace
}  // namespace esphome::rtsp_server
