#include "rtp_packetizer.h"
#include "rtp_packet.h"
#include "esphome/components/camera_video/h264_annexb.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace esphome::rtsp_server {
ESPHOME_LOG_TAG(TAG, "rtsp_server");
static constexpr uint8_t RTP_PAYLOAD_TYPE_JPEG = 26;
static constexpr uint8_t RTP_PAYLOAD_TYPE_H264 = 96;
static constexpr uint8_t H264_FU_A_NAL_TYPE = 28;
static constexpr uint8_t H264_FU_START_BIT = 0x80;
static constexpr uint8_t H264_FU_END_BIT = 0x40;
static constexpr size_t H264_FU_HEADER_LEN = 2;
static constexpr size_t BASE64_PARAMETER_SET_BYTES = 4 * ((camera_video::H264_PARAMETER_SET_MAX_SIZE + 2) / 3) + 1;
namespace {
size_t encode_base64_to(char *output, size_t cap, const uint8_t *src, size_t length) {
  static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t needed = 4 * ((length + 2) / 3);
  if (output == nullptr || src == nullptr || length == 0 || needed >= cap)
    return 0;
  size_t at = 0;
  for (size_t i = 0; i < length; i += 3) {
    const uint32_t a = src[i];
    const uint32_t b = i + 1 < length ? src[i + 1] : 0;
    const uint32_t c = i + 2 < length ? src[i + 2] : 0;
    output[at++] = ALPHABET[a >> 2];
    output[at++] = ALPHABET[((a & 3U) << 4) | (b >> 4)];
    output[at++] = i + 1 < length ? ALPHABET[((b & 15U) << 2) | (c >> 6)] : '=';
    output[at++] = i + 2 < length ? ALPHABET[c & 63U] : '=';
  }
  output[at] = 0;
  return at;
}
}  // namespace
/// Scans JPEG marker segments (SOF0/1, DQT, SOS) for the dimensions, quantization tables and the start of
/// the entropy-coded scan data. Assumes the standard Huffman tables (true for esp32-camera), so DHT segments
/// are not parsed; receivers rebuild them per RFC 2435. Only three-component 4:2:2 or 4:2:0 frames without
/// restart markers (DRI) can be carried as RFC 2435 types 0 and 1; anything else returns false.
bool parse_jpeg_info(const uint8_t *data, size_t len, JpegInfo &out) {
  if (len < 4 || data[0] != 0xFF || data[1] != 0xD8)
    return false;  // no SOI

  size_t pos = 2;
  while (pos + 2 <= len) {
    if (data[pos] != 0xFF) {
      pos++;  // resync on a stray non-marker byte
      continue;
    }
    uint8_t marker = data[pos + 1];
    if (marker == 0xFF) {
      pos++;  // fill byte before the real marker
      continue;
    }
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD9)) {
      pos += 2;  // standalone marker, no length field (TEM, RSTn, SOI, EOI)
      continue;
    }
    if (pos + 4 > len)
      break;

    uint16_t seg_len = encode_uint16(data[pos + 2], data[pos + 3]);
    size_t seg_start = pos + 4;
    if (seg_len < 2 || seg_start + (seg_len - 2) > len)
      return false;
    size_t seg_end = seg_start + (seg_len - 2);

    if (marker == 0xC0 || marker == 0xC1) {  // SOF0 / SOF1
      if (seg_len < 8 + 3 * 3 || data[seg_start + 5] != 3)
        return false;  // not three components
      out.height = encode_uint16(data[seg_start + 1], data[seg_start + 2]);
      out.width = encode_uint16(data[seg_start + 3], data[seg_start + 4]);
      const uint8_t sampling = data[seg_start + 7];  // Y component H/V sampling factors
      if (sampling == 0x21) {
        out.type = 0;  // 4:2:2
      } else if (sampling == 0x22) {
        out.type = 1;  // 4:2:0
      } else {
        return false;
      }
    } else if (marker == 0xDD) {  // DRI
      return false;
    } else if (marker == 0xDB) {  // DQT, one or more tables back to back
      size_t p = seg_start;
      while (p < seg_end) {
        uint8_t precision = data[p] >> 4;
        uint8_t table_id = data[p] & 0x0F;
        size_t table_len = precision == 0 ? QTABLE_LEN : 2 * QTABLE_LEN;
        if (p + 1 + table_len > seg_end)
          break;
        if (precision == 0 && table_id == 0) {
          out.qtable0 = &data[p + 1];
        } else if (precision == 0 && table_id == 1) {
          out.qtable1 = &data[p + 1];
        }
        p += 1 + table_len;
      }
    } else if (marker == 0xDA) {  // SOS, scan data follows
      if (out.width == 0 || out.height == 0 || out.qtable0 == nullptr)
        return false;
      if (out.qtable1 == nullptr)
        out.qtable1 = out.qtable0;
      out.scan_data = data + seg_end;
      out.scan_len = len - seg_end;
      if (out.scan_len >= 2 && out.scan_data[out.scan_len - 2] == 0xFF && out.scan_data[out.scan_len - 1] == 0xD9)
        out.scan_len -= 2;  // the RFC 2435 payload is the entropy-coded data only
      return true;
    }

    pos = seg_end;
  }
  return false;
}

std::unique_ptr<RtpPacketizer> make_rtp_packetizer(camera_video::EncodedVideoStream *stream, uint16_t payload_size) {
  camera_video::EncodedVideoFormat format;
  if (stream == nullptr || !stream->get_format(&format))
    return nullptr;
  if (format.codec == camera_video::VideoCodec::H264)
    return std::unique_ptr<RtpPacketizer>(new RtpH264Packetizer(stream, payload_size));
  return std::unique_ptr<RtpPacketizer>(new RtpJpegPacketizer());
}

bool RtpJpegPacketizer::write_sdp(char *data, size_t size) const {
  static constexpr char SDP[] =
      "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=ESPHome RTSP\r\nt=0 0\r\nm=video 0 RTP/AVP 26\r\na=control:trackID=0\r\n";
  if (data == nullptr || size < sizeof(SDP))
    return false;
  std::memcpy(data, SDP, sizeof(SDP));
  return true;
}
bool RtpJpegPacketizer::set_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) {
  if (frame == nullptr || this->frame_ != nullptr)
    return false;
  JpegInfo info;
  if (!parse_jpeg_info(frame->get_data_buffer(), frame->get_data_length(), info) ||
      info.width > RTP_JPEG_MAX_DIMENSION || info.height > RTP_JPEG_MAX_DIMENSION)
    return false;
  this->frame_ = frame;
  this->jpeg_ = info;
  this->frame_sent_ = 0;
  return true;
}
bool RtpJpegPacketizer::next_packet(RtpPacketContext &context) {
  if (this->frame_ == nullptr)
    return false;
  if (this->frame_sent_ >= this->jpeg_.scan_len) {
    this->frame_.reset();  // its last packet is written
    return false;
  }

  const bool first_fragment = this->frame_sent_ == 0;
  const size_t header_len =
      INTERLEAVED_HEADER_LEN + RTP_HEADER_LEN + JPEG_HEADER_LEN + (first_fragment ? QUANT_HEADER_LEN : 0);
  const size_t qtables_len = first_fragment ? 2 * QTABLE_LEN : 0;
  const size_t payload_len =
      std::min(this->jpeg_.scan_len - this->frame_sent_, MAX_PACKET_SIZE - header_len - qtables_len);
  const bool last_fragment = this->frame_sent_ + payload_len >= this->jpeg_.scan_len;
  auto *buf = reinterpret_cast<uint8_t *>(context.buffer);
  const size_t packet_payload_len =
      JPEG_HEADER_LEN + (first_fragment ? QUANT_HEADER_LEN : 0) + qtables_len + payload_len;
  size_t pos = write_interleaved_rtp_header(buf, 0, RTP_PAYLOAD_TYPE_JPEG, last_fragment, packet_payload_len,
                                            context.sequence++, context.timestamp, context.ssrc);
  if (pos == 0)
    return false;

  // RFC 2435 main JPEG header
  buf[pos++] = 0;  // type-specific
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_ >> 16);
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_ >> 8);
  buf[pos++] = static_cast<uint8_t>(this->frame_sent_);
  buf[pos++] = this->jpeg_.type;
  buf[pos++] = 255;  // Q >= 128: the quantization tables follow in the first packet
  buf[pos++] = static_cast<uint8_t>(this->jpeg_.width / 8);
  buf[pos++] = static_cast<uint8_t>(this->jpeg_.height / 8);
  uint8_t count = 0;
  if (first_fragment) {
    buf[pos++] = 0;  // MBZ
    buf[pos++] = 0;  // 8-bit precision
    buf[pos++] = static_cast<uint8_t>(qtables_len >> 8);
    buf[pos++] = static_cast<uint8_t>(qtables_len);
    context.iov[0] = {buf, header_len};
    context.iov[1] = {const_cast<uint8_t *>(this->jpeg_.qtable0), QTABLE_LEN};
    context.iov[2] = {const_cast<uint8_t *>(this->jpeg_.qtable1), QTABLE_LEN};
    count = 3;
  } else {
    context.iov[0] = {buf, header_len};
    count = 1;
  }
  context.iov[count++] = {const_cast<uint8_t *>(this->jpeg_.scan_data + this->frame_sent_), payload_len};
  context.count = count;
  this->frame_sent_ += payload_len;
  return true;
}

bool RtpH264Packetizer::set_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) {
  if (this->stream_ == nullptr || frame == nullptr)
    return false;
  if (this->has_frame()) {
    this->discontinuity();
    return false;
  }
  if (this->waiting_for_keyframe_ && !frame->is_keyframe())
    return false;
  if (frame->is_keyframe())
    this->waiting_for_keyframe_ = false;

  this->h264_frame_ = frame;
  this->h264_cursor_ = frame->get_data_buffer();
  this->h264_end_ = this->h264_cursor_ + frame->get_data_length();
  this->h264_nal_data_ = nullptr;
  this->h264_nal_length_ = 0;
  this->h264_nal_offset_ = 0;
  if (frame->is_keyframe() && this->read_config_(&this->h264_codec_config_)) {
    this->h264_stage_ = H264Stage::CONFIG_SPS;
  } else {
    this->h264_stage_ = H264Stage::FRAME;
  }

  return true;
}
void RtpH264Packetizer::reset() {
  this->clear_h264_frame_();
  this->waiting_for_keyframe_ = true;
}
bool RtpH264Packetizer::read_config_(camera_video::H264CodecConfig *config) const {
  if (config == nullptr || this->stream_ == nullptr)
    return false;
  std::array<uint8_t, 2 * (camera_video::H264_PARAMETER_SET_MAX_SIZE + 4)> data{};
  size_t length = 0;
  if (!this->stream_->get_codec_config(data.data(), data.size(), &length) || length > data.size())
    return false;
  const uint8_t *cursor = data.data();
  const uint8_t *end = cursor + length;
  camera_video::H264AnnexBNal nal;
  while (camera_video::next_annexb_nal(&cursor, end, &nal)) {
    if (nal.len == 0 || nal.len > camera_video::H264_PARAMETER_SET_MAX_SIZE)
      continue;
    const uint8_t type = nal.data[0] & camera_video::H264_NAL_TYPE_MASK;
    if (type == 7) {
      std::memcpy(config->sps.data(), nal.data, nal.len);
      config->sps_length = nal.len;
    } else if (type == 8) {
      std::memcpy(config->pps.data(), nal.data, nal.len);
      config->pps_length = nal.len;
    }
  }
  if (!config->is_valid())
    return false;
  config->profile_level_id =
      (static_cast<uint32_t>(config->sps[1]) << 16) | (static_cast<uint32_t>(config->sps[2]) << 8) | config->sps[3];
  return true;
}

bool RtpH264Packetizer::write_sdp(char *data, size_t size) const {
  camera_video::H264CodecConfig config;
  camera_video::EncodedVideoFormat info;
  if (data == nullptr || !this->read_config_(&config) || !this->stream_->get_format(&info))
    return false;
  char sps[BASE64_PARAMETER_SET_BYTES];
  char pps[BASE64_PARAMETER_SET_BYTES];
  if (encode_base64_to(sps, sizeof(sps), config.sps.data(), config.sps_length) == 0 ||
      encode_base64_to(pps, sizeof(pps), config.pps.data(), config.pps_length) == 0)
    return false;
  const int length =
      snprintf(data, size,
               "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=ESPHome RTSP\r\nt=0 0\r\n"
               "m=video 0 RTP/AVP 96\r\nc=IN IP4 0.0.0.0\r\na=rtpmap:96 H264/90000\r\n"
               "a=framerate:%u\r\na=fmtp:96 packetization-mode=1;profile-level-id=%06" PRIX32
               ";sprop-parameter-sets=%s,%s\r\na=control:trackID=0\r\n",
               static_cast<unsigned>(info.fps_num / info.fps_den), config.profile_level_id & 0xFFFFFFU, sps, pps);
  return length > 0 && static_cast<size_t>(length) < size;
}

bool RtpH264Packetizer::load_next_h264_nal_() {
  while (true) {
    this->h264_nal_offset_ = 0;
    this->h264_nal_last_ = false;
    if (this->h264_stage_ == H264Stage::CONFIG_SPS) {
      this->h264_nal_data_ = this->h264_codec_config_.sps.data();
      this->h264_nal_length_ = this->h264_codec_config_.sps_length;
      this->h264_stage_ = H264Stage::CONFIG_PPS;
      return this->h264_nal_length_ != 0;
    }
    if (this->h264_stage_ == H264Stage::CONFIG_PPS) {
      this->h264_nal_data_ = this->h264_codec_config_.pps.data();
      this->h264_nal_length_ = this->h264_codec_config_.pps_length;
      this->h264_stage_ = H264Stage::FRAME;
      return this->h264_nal_length_ != 0;
    }
    if (this->h264_stage_ != H264Stage::FRAME || this->h264_frame_ == nullptr) {
      this->clear_h264_frame_();
      return false;
    }

    camera_video::H264AnnexBNal nal;
    if (!camera_video::next_annexb_nal(&this->h264_cursor_, this->h264_end_, &nal)) {
      this->clear_h264_frame_();
      return false;
    }
    this->h264_nal_data_ = nal.data;
    this->h264_nal_length_ = nal.len;
    const uint8_t *peek = this->h264_cursor_;
    camera_video::H264AnnexBNal next;
    this->h264_nal_last_ = !camera_video::next_annexb_nal(&peek, this->h264_end_, &next);
    if (this->h264_nal_length_ != 0)
      return true;
  }
}

bool RtpH264Packetizer::next_packet(RtpPacketContext &context) {
  if (this->h264_frame_ == nullptr)
    return false;
  if (this->h264_nal_data_ == nullptr && !this->load_next_h264_nal_())
    return false;

  auto *buf = reinterpret_cast<uint8_t *>(context.buffer);
  if (this->h264_nal_length_ <= this->h264_payload_size_) {
    const bool marker = this->h264_nal_last_;
    const size_t header_len =
        write_interleaved_rtp_header(buf, 0, RTP_PAYLOAD_TYPE_H264, marker, this->h264_nal_length_, context.sequence++,
                                     context.timestamp, context.ssrc);
    context.iov[0] = {buf, header_len};
    context.iov[1] = {const_cast<uint8_t *>(this->h264_nal_data_), this->h264_nal_length_};
    context.count = 2;
    this->h264_nal_data_ = nullptr;
    this->h264_nal_length_ = 0;
    return true;
  }

  if (this->h264_nal_length_ <= 1 || this->h264_payload_size_ <= H264_FU_HEADER_LEN) {
    ESP_LOGW(TAG, "Invalid H.264 NAL/payload size (%zu/%zu)", this->h264_nal_length_, this->h264_payload_size_);
    this->discontinuity();
    this->clear_h264_frame_();
    return false;
  }

  const size_t maximum_fragment = this->h264_payload_size_ - H264_FU_HEADER_LEN;
  const size_t payload_bytes = this->h264_nal_length_ - 1;
  const size_t remaining = payload_bytes - this->h264_nal_offset_;
  const size_t fragment_length = std::min(remaining, maximum_fragment);
  const bool first_fragment = this->h264_nal_offset_ == 0;
  const bool last_fragment = fragment_length == remaining;
  const bool marker = this->h264_nal_last_ && last_fragment;
  const uint8_t nal_header = this->h264_nal_data_[0];
  const size_t header_len =
      write_interleaved_rtp_header(buf, 0, RTP_PAYLOAD_TYPE_H264, marker, H264_FU_HEADER_LEN + fragment_length,
                                   context.sequence++, context.timestamp, context.ssrc);
  buf[header_len] = static_cast<uint8_t>((nal_header & 0xE0U) | H264_FU_A_NAL_TYPE);
  buf[header_len + 1] =
      static_cast<uint8_t>((nal_header & camera_video::H264_NAL_TYPE_MASK) | (first_fragment ? H264_FU_START_BIT : 0U) |
                           (last_fragment ? H264_FU_END_BIT : 0U));
  context.iov[0] = {buf, header_len + H264_FU_HEADER_LEN};
  context.iov[1] = {const_cast<uint8_t *>(this->h264_nal_data_ + 1 + this->h264_nal_offset_), fragment_length};
  context.count = 2;
  this->h264_nal_offset_ += fragment_length;
  if (last_fragment) {
    this->h264_nal_data_ = nullptr;
    this->h264_nal_length_ = 0;
    this->h264_nal_offset_ = 0;
  }
  return true;
}

void RtpH264Packetizer::discontinuity() {
  if (this->waiting_for_keyframe_)
    return;
  this->waiting_for_keyframe_ = true;
  if (this->stream_ != nullptr)
    this->stream_->request_keyframe();
}

void RtpH264Packetizer::clear_h264_frame_() {
  this->h264_frame_.reset();
  this->h264_cursor_ = nullptr;
  this->h264_end_ = nullptr;
  this->h264_nal_data_ = nullptr;
  this->h264_nal_length_ = 0;
  this->h264_nal_offset_ = 0;
  this->h264_nal_last_ = false;
  this->h264_stage_ = H264Stage::DONE;
}
}  // namespace esphome::rtsp_server
