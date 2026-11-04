#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/uio.h>
#include "esphome/components/camera_video/encoded_stream.h"
#include "esphome/components/camera_video/h264_stream.h"
#include "esphome/components/socket/socket.h"

namespace esphome::rtsp_server {
static constexpr size_t INTERLEAVED_HEADER_LEN = 4;
static constexpr size_t RTP_HEADER_LEN = 12;
static constexpr size_t JPEG_HEADER_LEN = 8;
static constexpr size_t QUANT_HEADER_LEN = 4;
static constexpr size_t QTABLE_LEN = 64;
static constexpr size_t RTP_JPEG_PAYLOAD_SIZE = 1024;
static constexpr size_t MAX_PACKET_SIZE = INTERLEAVED_HEADER_LEN + RTP_HEADER_LEN + JPEG_HEADER_LEN + QUANT_HEADER_LEN +
                                          2 * QTABLE_LEN + RTP_JPEG_PAYLOAD_SIZE;
static constexpr uint16_t RTP_JPEG_MAX_DIMENSION = 255 * 8;
static constexpr uint32_t RTP_CLOCK_RATE = 90000;

struct JpegInfo {
  const uint8_t *qtable0{nullptr};
  const uint8_t *qtable1{nullptr};
  const uint8_t *scan_data{nullptr};
  size_t scan_len{0};
  uint16_t width{0};
  uint16_t height{0};
  uint8_t type{0};
};
bool parse_jpeg_info(const uint8_t *data, size_t len, JpegInfo &out);

// Packet data may reference frame storage; RTSP must finish a packet before releasing the frame.
struct RtpPacketContext {
  uint8_t *buffer;
  std::array<struct iovec, 4> &iov;
  uint8_t &count;
  uint16_t &sequence;
  uint32_t timestamp;
  uint32_t ssrc;
};

class RtpPacketizer {
 public:
  virtual ~RtpPacketizer() = default;
  virtual bool write_sdp(char *data, size_t size) const = 0;
  virtual bool set_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) = 0;
  virtual bool next_packet(RtpPacketContext &context) = 0;
  virtual bool has_frame() const = 0;
  virtual void reset() = 0;
  virtual bool awaiting_keyframe() const { return false; }
  virtual void discontinuity() {}
};

std::unique_ptr<RtpPacketizer> make_rtp_packetizer(camera_video::EncodedVideoStream *stream, uint16_t payload_size);

class RtpJpegPacketizer final : public RtpPacketizer {
 public:
  bool write_sdp(char *data, size_t size) const override;
  bool set_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) override;
  bool next_packet(RtpPacketContext &context) override;
  bool has_frame() const override { return this->frame_ != nullptr; }
  void reset() override { this->frame_.reset(); }

 private:
  camera_video::FrameRef<camera_video::EncodedVideoFrame> frame_;
  JpegInfo jpeg_;
  size_t frame_sent_{0};
};

class RtpH264Packetizer final : public RtpPacketizer {
 public:
  explicit RtpH264Packetizer(camera_video::EncodedVideoStream *stream, uint16_t payload_size)
      : stream_(stream), h264_payload_size_(payload_size) {}
  bool write_sdp(char *data, size_t size) const override;
  bool set_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) override;
  bool next_packet(RtpPacketContext &context) override;
  bool has_frame() const override { return this->h264_frame_ != nullptr; }
  void reset() override;
  bool awaiting_keyframe() const override { return this->waiting_for_keyframe_; }
  void discontinuity() override;

 private:
  enum class H264Stage : uint8_t { CONFIG_SPS, CONFIG_PPS, FRAME, DONE };
  bool load_next_h264_nal_();
  bool read_config_(camera_video::H264CodecConfig *config) const;
  void clear_h264_frame_();
  camera_video::EncodedVideoStream *stream_;
  camera_video::FrameRef<camera_video::EncodedVideoFrame> h264_frame_;
  camera_video::H264CodecConfig h264_codec_config_;
  const uint8_t *h264_cursor_{nullptr};
  const uint8_t *h264_end_{nullptr};
  const uint8_t *h264_nal_data_{nullptr};
  size_t h264_nal_length_{0};
  size_t h264_nal_offset_{0};
  uint16_t h264_payload_size_;
  H264Stage h264_stage_{H264Stage::DONE};
  bool h264_nal_last_{false};
  bool waiting_for_keyframe_{true};
};

}  // namespace esphome::rtsp_server
