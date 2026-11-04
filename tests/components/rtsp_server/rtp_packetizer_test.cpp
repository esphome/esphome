#include "esphome/components/rtsp_server/rtp_packetizer.h"
#include "gtest/gtest.h"
#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace esphome::rtsp_server {
namespace {

TEST(RtpPacketizer, LegacyJpegSdp) {
  RtpJpegPacketizer packetizer;
  std::array<char, 256> sdp{};
  ASSERT_TRUE(packetizer.write_sdp(sdp.data(), sdp.size()));
  EXPECT_NE(std::strstr(sdp.data(), "RTP/AVP 26"), nullptr);
  EXPECT_FALSE(packetizer.has_frame());
}

TEST(RtpPacketizer, JpegRejectsInvalidFrame) {
  RtpJpegPacketizer packetizer;
  EXPECT_FALSE(packetizer.set_frame({}));
  EXPECT_FALSE(packetizer.has_frame());
  packetizer.reset();
  EXPECT_FALSE(packetizer.has_frame());
}

TEST(RtpPacketizer, H264RequiresCodecConfig) {
  RtpH264Packetizer packetizer(nullptr, 1400);
  std::array<char, 768> sdp{};
  EXPECT_FALSE(packetizer.write_sdp(sdp.data(), sdp.size()));
  EXPECT_TRUE(packetizer.awaiting_keyframe());
  EXPECT_FALSE(packetizer.set_frame({}));
  packetizer.discontinuity();
  EXPECT_TRUE(packetizer.awaiting_keyframe());
}

class TestFrame final : public camera_video::EncodedVideoFrame {
 public:
  explicit TestFrame(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
  uint8_t *get_data_buffer() override { return this->bytes_.data(); }
  size_t get_data_length() override { return this->bytes_.size(); }
  uint64_t get_timestamp_us() const override { return 1000000; }
  bool is_keyframe() const override { return true; }

 protected:
  void on_last_reference() override { delete this; }

 private:
  std::vector<uint8_t> bytes_;
};

class TestH264Stream final : public camera_video::EncodedVideoStream {
 public:
  bool get_format(camera_video::EncodedVideoFormat *format) const override {
    if (format == nullptr)
      return false;
    *format = {camera_video::VideoCodec::H264, 640, 480, 30, 1};
    return true;
  }
  bool get_codec_config(uint8_t *data, size_t capacity, size_t *length) const override {
    static constexpr uint8_t CONFIG[] = {0, 0, 0, 1, 0x67, 0x42, 0, 0x1e, 0, 0, 0, 1, 0x68, 0xee};
    if (length == nullptr)
      return false;
    *length = sizeof(CONFIG);
    if (data == nullptr)
      return capacity == 0;
    if (capacity < sizeof(CONFIG))
      return false;
    std::memcpy(data, CONFIG, sizeof(CONFIG));
    return true;
  }
  bool start_stream(camera_video::EncodedVideoStreamListener *) override { return true; }
  void stop_stream(camera_video::EncodedVideoStreamListener *) override {}
  bool is_ready() const override { return true; }
  bool request_keyframe() override { return true; }
};

TEST(RtpPacketizer, H264SdpAndFuAFragments) {
  TestH264Stream source;
  RtpH264Packetizer packetizer(&source, 5);
  std::array<char, 768> sdp{};
  ASSERT_TRUE(packetizer.write_sdp(sdp.data(), sdp.size()));
  EXPECT_NE(std::strstr(sdp.data(), "packetization-mode=1"), nullptr);
  EXPECT_NE(std::strstr(sdp.data(), "profile-level-id=42001E"), nullptr);

  camera_video::FrameRef<camera_video::EncodedVideoFrame> frame(
      new TestFrame({0, 0, 0, 1, 0x65, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
  ASSERT_TRUE(packetizer.set_frame(frame));
  std::array<uint8_t, 256> header{};
  std::array<struct iovec, 4> iov{};
  uint8_t count = 0;
  uint16_t seq = 100;
  unsigned fu_packets = 0;
  bool first_fu = false;
  bool last_fu = false;
  for (unsigned i = 0; i < 20; i++) {
    RtpPacketContext context{header.data(), iov, count, seq, 90000, 123};
    if (!packetizer.next_packet(context))
      break;
    ASSERT_GT(count, 0);
    if (iov[0].iov_len == 18 && (header[16] & 0x1f) == 28) {
      fu_packets++;
      first_fu |= (header[17] & 0x80) != 0;
      last_fu |= (header[17] & 0x40) != 0;
    }
  }
  EXPECT_GT(fu_packets, 1U);
  EXPECT_TRUE(first_fu);
  EXPECT_TRUE(last_fu);
  EXPECT_FALSE(packetizer.has_frame());
}

}  // namespace
}  // namespace esphome::rtsp_server
