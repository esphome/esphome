#include "esphome/core/defines.h"
#ifdef USE_RTSP_SERVER

#include "h264_rtsp_track.h"
#include "h264_rtp_packetizer.h"
#include "esphome/components/camera_video/h264_annexb.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <esp_timer.h>
#include <esp_tls_crypto.h>

namespace esphome::rtsp_server {

static const char *const TAG = "h264_rtsp_track";
static constexpr uint32_t RTP_CLOCK_HZ = 90000;
static constexpr uint32_t MICROSECONDS_PER_SECOND = 1000000;
static constexpr uint8_t RTP_PAYLOAD_TYPE_H264 = 96;
static constexpr size_t BASE64_PARAMETER_SET_BYTES = 4U * ((camera_video::H264_PARAMETER_SET_MAX_SIZE + 2U) / 3U) + 1U;

static size_t encode_base64_to(char *output, size_t output_size, const uint8_t *input, size_t input_size) {
  if (output == nullptr || output_size == 0 || input == nullptr || input_size == 0) {
    return 0;
  }
  size_t encoded_length = 0;
  if (esp_crypto_base64_encode(reinterpret_cast<uint8_t *>(output), output_size, &encoded_length, input, input_size) !=
          0 ||
      encoded_length >= output_size) {
    output[0] = '\0';
    return 0;
  }
  output[encoded_length] = '\0';
  return encoded_length;
}

H264RtspTrack::~H264RtspTrack() {
  if (this->subscribed_.exchange(false, std::memory_order_acq_rel) && this->stream_ != nullptr) {
    this->stream_->stop_stream(this);
  }
  this->listener_.store(nullptr, std::memory_order_release);
  this->purge();
}

bool H264RtspTrack::start(RtspTrackListener *listener) {
  if (listener == nullptr || this->stream_ == nullptr) {
    return false;
  }
  RtspTrackListener *expected = nullptr;
  if (!this->listener_.compare_exchange_strong(expected, listener, std::memory_order_acq_rel)) {
    return expected == listener && this->subscribed_.load(std::memory_order_acquire);
  }
  if (this->subscribed_.exchange(true, std::memory_order_acq_rel)) {
    return true;
  }
  if (this->stream_->start_stream(this)) {
    return true;
  }
  this->subscribed_.store(false, std::memory_order_release);
  expected = listener;
  (void) this->listener_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
  return false;
}

void H264RtspTrack::stop(RtspTrackListener *listener) {
  RtspTrackListener *expected = listener;
  if (!this->listener_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
    return;
  }
  if (this->subscribed_.exchange(false, std::memory_order_acq_rel) && this->stream_ != nullptr) {
    this->stream_->stop_stream(this);
  }
  this->purge();
}

bool H264RtspTrack::is_ready() const { return this->stream_ != nullptr && this->stream_->is_ready(); }

bool H264RtspTrack::is_sdp_ready() const {
  if (this->stream_ == nullptr) {
    return false;
  }
  camera_video::H264CodecConfig config;
  return this->stream_->get_h264_codec_config(&config);
}

uint32_t H264RtspTrack::get_frame_period_ms() const {
  if (this->stream_ == nullptr) {
    return 0;
  }
  const auto info = this->stream_->get_stream_info();
  return info.fps == 0 ? 0 : static_cast<uint32_t>((1000U + info.fps - 1U) / info.fps);
}

bool H264RtspTrack::write_sdp(char *buffer, size_t buffer_size, const char *control_path) const {
  if (buffer == nullptr || buffer_size == 0 || control_path == nullptr || this->stream_ == nullptr) {
    return false;
  }
  camera_video::H264CodecConfig config;
  if (!this->stream_->get_h264_codec_config(&config)) {
    return false;
  }
  const auto info = this->stream_->get_stream_info();
  char sps[BASE64_PARAMETER_SET_BYTES];
  char pps[BASE64_PARAMETER_SET_BYTES];
  if (encode_base64_to(sps, sizeof(sps), config.sps.data(), config.sps_length) == 0 ||
      encode_base64_to(pps, sizeof(pps), config.pps.data(), config.pps_length) == 0) {
    return false;
  }
  const int length =
      snprintf(buffer, buffer_size,
               "m=video 0 RTP/AVP %u\r\n"
               "c=IN IP4 0.0.0.0\r\n"
               "a=rtpmap:%u H264/%" PRIu32 "\r\n"
               "a=framerate:%u\r\n"
               "a=fmtp:%u packetization-mode=1;profile-level-id=%06" PRIX32 ";sprop-parameter-sets=%s,%s\r\n"
               "a=control:%s\r\n",
               RTP_PAYLOAD_TYPE_H264, RTP_PAYLOAD_TYPE_H264, RTP_CLOCK_HZ, static_cast<unsigned>(info.fps),
               RTP_PAYLOAD_TYPE_H264, config.profile_level_id & 0xFFFFFFU, sps, pps, control_path);
  return length > 0 && static_cast<size_t>(length) < buffer_size;
}

void H264RtspTrack::on_h264_frame(const camera_video::FrameRef<camera_video::H264Frame> &frame) {
  if (!this->subscribed_.load(std::memory_order_acquire) || frame == nullptr) {
    return;
  }
  if (!this->frame_mutex_.try_lock()) {
    this->dropped_frames_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const bool queue = this->pending_frame_ == nullptr || !this->pending_frame_->is_keyframe() || frame->is_keyframe();
  if (queue) {
    this->pending_frame_ = frame;
  } else {
    this->dropped_frames_.fetch_add(1, std::memory_order_relaxed);
  }
  this->frame_mutex_.unlock();
  if (queue) {
    RtspTrackListener *listener = this->listener_.load(std::memory_order_acquire);
    if (listener != nullptr) {
      listener->on_rtsp_track_frame_available();
    }
  }
}

camera_video::FrameRef<camera_video::H264Frame> H264RtspTrack::take_pending_frame_() {
  LockGuard lock(this->frame_mutex_);
  camera_video::FrameRef<camera_video::H264Frame> frame;
  frame.swap(this->pending_frame_);
  return frame;
}

void H264RtspTrack::purge() {
  LockGuard lock(this->frame_mutex_);
  this->pending_frame_.reset();
}

void H264RtspTrack::request_keyframe() {
  if (this->stream_ != nullptr) {
    this->stream_->request_keyframe();
  }
}

bool H264RtspTrack::send_nal_(RtpPacketSink &sink, const uint8_t *nal, size_t length, bool marker,
                              bool startup_config) const {
  return packetize_h264_nal(
      nal, length, sink.get_maximum_payload_size(), marker, [&sink, startup_config](const H264RtpFragment &fragment) {
        const RtpPacketView packet{RTP_PAYLOAD_TYPE_H264,   fragment.prefix, fragment.prefix_length, fragment.payload,
                                   fragment.payload_length, fragment.marker, startup_config};
        return sink.send_packet(packet);
      });
}

RtspTrackSendResult H264RtspTrack::send_next_access_unit(RtpPacketSink &sink) {
  auto frame = this->take_pending_frame_();
  if (frame == nullptr) {
    return RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_NO_FRAME;
  }
  const uint64_t timestamp_us =
      frame->get_timestamp_us() == 0 ? static_cast<uint64_t>(esp_timer_get_time()) : frame->get_timestamp_us();
  const uint32_t timestamp = static_cast<uint32_t>(timestamp_us * RTP_CLOCK_HZ / MICROSECONDS_PER_SECOND);
  if (!sink.begin_access_unit(timestamp, frame->is_keyframe(), frame->get_data_length())) {
    return RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_SKIPPED;
  }

  if (frame->is_keyframe()) {
    camera_video::H264CodecConfig config;
    if (this->stream_->get_h264_codec_config(&config) &&
        (!this->send_nal_(sink, config.sps.data(), config.sps_length, false, true) ||
         !this->send_nal_(sink, config.pps.data(), config.pps_length, false, true))) {
      return RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_FAILED;
    }
  }

  const uint8_t *cursor = frame->get_data_buffer();
  const uint8_t *end = cursor + frame->get_data_length();
  camera_video::H264AnnexBNal nal;
  bool sent = false;
  while (camera_video::next_annexb_nal(&cursor, end, &nal)) {
    sent = true;
    if (!this->send_nal_(sink, nal.data, nal.len, cursor == end, false)) {
      return RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_FAILED;
    }
  }
  if (!sent) {
    ESP_LOGW(TAG, "H.264 access unit contained no Annex-B NAL units");
    return RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_FAILED;
  }
  return sink.end_access_unit() ? RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_SENT
                                : RtspTrackSendResult::RTSP_TRACK_SEND_RESULT_FAILED;
}

}  // namespace esphome::rtsp_server
#endif
