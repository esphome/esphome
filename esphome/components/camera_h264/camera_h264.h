#pragma once
#if defined(USE_CAMERA_H264) && defined(USE_ESP32) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/components/camera_video/camera_source.h"
#include "esphome/components/camera_video/h264_stream.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

struct esp_h264_enc_if;
using esp_h264_enc_handle_t = esp_h264_enc_if *;

namespace esphome::camera_h264 {

using H264InputFormat = camera_video::VideoPixelFormat;
static constexpr size_t H264_MAX_ENCODED_FRAME_BUFFERS = 4;

class H264EncodedFrame final : public camera_video::H264Frame {
 public:
  uint8_t *get_data_buffer() override { return this->data_.load(std::memory_order_acquire); }
  size_t get_data_length() override { return this->length_; }
  uint64_t get_timestamp_us() const override { return this->timestamp_us_; }
  bool is_keyframe() const override { return this->keyframe_; }
  ~H264EncodedFrame() override = default;

 protected:
  void on_last_reference() override;

 private:
  friend class CameraH264Encoder;
  bool allocate(size_t capacity);
  bool is_available() const {
    return this->data_.load(std::memory_order_acquire) != nullptr && !this->has_references();
  }
  camera_video::FrameRef<camera_video::H264Frame> commit(size_t length, uint64_t timestamp_us, bool keyframe) {
    this->length_ = length;
    this->timestamp_us_ = timestamp_us;
    this->keyframe_ = keyframe;
    return camera_video::FrameRef<camera_video::H264Frame>(this);
  }
  void release_storage();
  void free_storage_();
  size_t capacity() const { return this->capacity_; }

  std::atomic<uint8_t *> data_{nullptr};
  size_t capacity_{0};
  size_t length_{0};
  uint64_t timestamp_us_{0};
  bool keyframe_{false};
  std::atomic<bool> release_pending_{false};
};

class CameraH264Encoder final : public Component,
                                public camera_video::CameraVideoSourceListener,
                                public camera_video::H264Stream {
 public:
  explicit CameraH264Encoder(camera_video::CameraVideoSource *video_source);
  void set_fps(uint16_t fps) {
    this->fps_ = fps;
    this->fps_configured_ = true;
  }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_gop(uint16_t gop) {
    this->gop_ = gop;
    this->gop_configured_ = true;
  }
  void set_qp_min(uint8_t qp_min) { this->qp_min_ = qp_min; }
  void set_qp_max(uint8_t qp_max) { this->qp_max_ = qp_max; }
  void set_encoded_frame_buffers(uint8_t count) { this->encoded_frame_buffer_count_ = count; }
  void set_output_buffer_size(size_t size) { this->configured_output_buffer_size_ = size; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool teardown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1; }

  void on_video_frame(const camera_video::FrameRef<camera_video::CameraVideoFrame> &frame) override;

  bool start_stream(camera_video::H264StreamListener *listener) override;
  void stop_stream(camera_video::H264StreamListener *listener) override;
  bool is_ready() const override { return !this->is_failed() && this->initialized_.load(std::memory_order_acquire); }
  camera_video::H264StreamInfo get_stream_info() const override;
  bool get_h264_codec_config(camera_video::H264CodecConfig *config) const override;
  void request_keyframe() override { this->force_idr_requested_.store(true, std::memory_order_release); }

  ~CameraH264Encoder() override;

 protected:
  bool apply_video_source_contract_(const camera_video::CameraVideoFrameSpec &spec);
  bool validate_configuration_() const;
  bool initialize_encoder_(const camera_video::CameraVideoFrameSpec &spec);
  bool init_encoder_();
  bool allocate_encoded_frames_();
  void release_encoded_frames_();
  H264EncodedFrame *acquire_encoded_frame_();
  void start_encoder_task_();
  void request_encoder_task_stop_();
  bool finish_encoder_task_stop_(bool force);
  void notify_encoder_task_();
  void encoder_task_body_();
  static void encoder_task_entry(void *arg);
  bool queue_video_frame_(const camera_video::FrameRef<camera_video::CameraVideoFrame> &frame);
  bool encode_stream_frame_(const uint8_t *data, size_t length, uint64_t timestamp_us);
  bool encode_into_buffer_(const uint8_t *data, size_t length, uint64_t timestamp_us, uint8_t *output_data,
                           size_t output_capacity, size_t *encoded_length, bool *keyframe);
  bool should_encode_live_frame_(uint64_t timestamp_us);
  void update_h264_codec_config_(const uint8_t *data, size_t length);
  void maybe_log_stats_(uint32_t now_us);
  bool remove_listener_(camera_video::H264StreamListener *listener);
  void publish_encoded_frame_(const camera_video::FrameRef<camera_video::H264Frame> &frame);
  void cleanup_encoder_resources_();

  camera_video::CameraVideoSource *video_source_{nullptr};
  H264InputFormat input_format_{H264InputFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY};
  uint16_t width_{0};
  uint16_t height_{0};
  uint16_t fps_{0};
  uint16_t source_fps_{0};
  bool fps_configured_{false};
  bool rate_limit_frames_{false};
  uint32_t bitrate_{2000000};
  uint16_t gop_{0};
  bool gop_configured_{false};
  uint8_t qp_min_{24};
  uint8_t qp_max_{40};
  uint8_t encoded_frame_buffer_count_{3};

  esp_h264_enc_handle_t enc_{nullptr};
  std::array<H264EncodedFrame, H264_MAX_ENCODED_FRAME_BUFFERS> encoded_frame_slots_{};
  size_t encoded_frame_capacity_{0};
  size_t configured_output_buffer_size_{0};

  std::atomic<bool> initialized_{false};
  std::atomic<bool> initialization_failed_{false};
  uint64_t next_frame_due_us_{0};
  std::atomic<bool> reset_frame_pacing_{false};
  bool input_mismatch_logged_{false};
  size_t input_frame_size_{0};
  std::atomic<bool> streaming_{false};
  std::atomic<bool> encoder_task_stop_{false};
  std::atomic<bool> encoder_task_exited_{false};
  std::atomic<bool> shutdown_started_{false};
  std::atomic<bool> force_idr_requested_{false};
  std::atomic<bool> stats_reset_requested_{true};

  Mutex frame_mutex_;
  StaticTask encoder_task_;
  camera_video::FrameRef<camera_video::CameraVideoFrame> pending_frame_;

  static constexpr size_t MAX_H264_LISTENERS = 4;
  mutable Mutex listeners_mutex_;
  StaticVector<camera_video::H264StreamListener *, MAX_H264_LISTENERS> listeners_;
  camera_video::H264CodecConfig codec_config_{};
  std::atomic<bool> codec_config_ready_{false};

  std::atomic<uint32_t> encoded_frames_{0};
  std::atomic<uint32_t> rate_limit_drops_{0};
  std::atomic<uint32_t> pending_frame_drops_{0};
  std::atomic<uint32_t> encoded_slot_drops_{0};
  std::atomic<uint32_t> encode_failures_{0};
  std::atomic<uint32_t> output_buffer_failures_{0};
  uint32_t stats_start_us_{0};
  uint32_t stats_last_encoded_{0};
  uint32_t stats_last_rate_limit_drops_{0};
  uint32_t stats_last_pending_frame_drops_{0};
  uint32_t stats_last_encoded_slot_drops_{0};
  uint32_t stats_last_encode_failures_{0};
  uint64_t stats_encode_time_us_{0};
  uint32_t stats_encode_time_max_us_{0};
  uint32_t stats_encode_samples_{0};
  size_t stats_encoded_max_bytes_{0};
};

}  // namespace esphome::camera_h264

#endif
