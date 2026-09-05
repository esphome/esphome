#include "esphome/core/defines.h"
#if defined(USE_CAMERA_H264) && defined(USE_ESP32) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "camera_h264.h"
#include "esphome/components/camera_video/h264_annexb.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "sdkconfig.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include <esp_h264_enc_single.h>
#include <esp_h264_enc_single_hw.h>
}

// esp_h264_alloc.h lacks extern "C" guards.
extern "C" {
#include <esp_h264_alloc.h>
}
#include <esp_heap_caps.h>

namespace esphome::camera_h264 {

static const char *const TAG = "h264_encoder";
static constexpr size_t H264_ALLOC_ALIGNMENT_BYTES = 64;
static constexpr size_t H264_OUTPUT_BUFFER_AUTO_BYTES = 512U * 1024U;
static constexpr uint32_t H264_OVERFLOW_LOG_INTERVAL = 100;
static constexpr uint8_t H264_STATS_DECIMAL_SCALE = 10;
static constexpr uint32_t MICROSECONDS_PER_SECOND = 1000000U;
static constexpr uint32_t H264_STATS_LOG_INTERVAL_US = 10U * MICROSECONDS_PER_SECOND;
static constexpr uint32_t LOW_BITRATE_WARN_BITS_PER_SEC = 8000000;
static constexpr uint8_t LOW_BITRATE_QP_MAX_WARN = 45;
static constexpr uint16_t H264_MAX_FPS = 60;
static constexpr uint16_t H264_MAX_GOP = UINT8_MAX;
static constexpr uint32_t stack_words(uint32_t bytes) {
  return (bytes + sizeof(StackType_t) - 1U) / sizeof(StackType_t);
}
static constexpr uint32_t ENCODER_TASK_STACK_SIZE = stack_words(12288);
static constexpr UBaseType_t ENCODER_TASK_PRIORITY = 5;

bool H264EncodedFrame::allocate(size_t capacity) {
  if (capacity == 0 || this->data_.load(std::memory_order_acquire) != nullptr) {
    return false;
  }
  uint32_t actual_size = 0;
  uint8_t *data = static_cast<uint8_t *>(
      esp_h264_aligned_malloc(H264_ALLOC_ALIGNMENT_BYTES, 1, capacity, &actual_size, ESP_H264_MEM_SPIRAM));
  if (data == nullptr || actual_size < capacity) {
    if (data != nullptr) {
      esp_h264_free(data);
    }
    return false;
  }
  this->capacity_ = capacity;
  this->release_pending_.store(false, std::memory_order_release);
  this->data_.store(data, std::memory_order_release);
  return true;
}

void H264EncodedFrame::release_storage() {
  this->release_pending_.store(true, std::memory_order_release);
  if (!this->has_references()) {
    this->free_storage_();
  }
}

void H264EncodedFrame::on_last_reference() {
  if (this->release_pending_.load(std::memory_order_acquire)) {
    this->free_storage_();
  }
}

void H264EncodedFrame::free_storage_() {
  uint8_t *data = this->data_.exchange(nullptr, std::memory_order_acq_rel);
  if (data != nullptr) {
    esp_h264_free(data);
    this->capacity_ = 0;
  }
}

CameraH264Encoder::CameraH264Encoder(camera_video::CameraVideoSource *video_source) : video_source_(video_source) {}

camera_video::H264StreamInfo CameraH264Encoder::get_stream_info() const {
  return {this->width_, this->height_, this->fps_, this->bitrate_};
}

bool CameraH264Encoder::get_h264_codec_config(camera_video::H264CodecConfig *config) const {
  if (config == nullptr || !this->codec_config_ready_.load(std::memory_order_acquire)) {
    return false;
  }
  *config = this->codec_config_;
  return true;
}

bool CameraH264Encoder::start_stream(camera_video::H264StreamListener *listener) {
  if (listener == nullptr || this->video_source_ == nullptr || this->is_failed()) {
    return false;
  }

  bool first_listener = false;
  {
    LockGuard lock(this->listeners_mutex_);
    if (std::find(this->listeners_.begin(), this->listeners_.end(), listener) != this->listeners_.end()) {
      return true;
    }
    if (this->listeners_.size() >= MAX_H264_LISTENERS) {
      ESP_LOGE(TAG, "Maximum H.264 listener count reached");
      return false;
    }
    first_listener = this->listeners_.empty();
    this->listeners_.push_back(listener);
  }

  if (!first_listener) {
    return true;
  }

  this->reset_frame_pacing_.store(true, std::memory_order_release);
  this->stats_reset_requested_.store(true, std::memory_order_release);
  this->streaming_.store(true, std::memory_order_release);
  if (!this->initialized_.load(std::memory_order_acquire)) {
    return true;
  }
  if (this->video_source_->start_stream(this)) {
    return true;
  }

  this->streaming_.store(false, std::memory_order_release);
  (void) this->remove_listener_(listener);
  ESP_LOGE(TAG, "Raw-video source rejected H.264 stream subscription");
  return false;
}

void CameraH264Encoder::stop_stream(camera_video::H264StreamListener *listener) {
  if (listener == nullptr || !this->remove_listener_(listener)) {
    return;
  }

  this->streaming_.store(false, std::memory_order_release);
  if (this->initialized_.load(std::memory_order_acquire)) {
    this->video_source_->stop_stream(this);
  }
  {
    LockGuard lock(this->frame_mutex_);
    this->pending_frame_.reset();
  }
  this->stats_reset_requested_.store(true, std::memory_order_release);
}

bool CameraH264Encoder::remove_listener_(camera_video::H264StreamListener *listener) {
  LockGuard lock(this->listeners_mutex_);
  if (std::find(this->listeners_.begin(), this->listeners_.end(), listener) == this->listeners_.end()) {
    return false;
  }
  StaticVector<camera_video::H264StreamListener *, MAX_H264_LISTENERS> retained;
  for (auto *registered : this->listeners_) {
    if (registered != listener) {
      retained.push_back(registered);
    }
  }
  this->listeners_.assign(retained.begin(), retained.end());
  return this->listeners_.empty();
}

static const char *h264_input_format_name(H264InputFormat format) {
  switch (format) {
    case H264InputFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY:
      return "O_UYY_E_VYY";
    case H264InputFormat::VIDEO_PIXEL_FORMAT_VUY:
      return "VUY";
    case H264InputFormat::VIDEO_PIXEL_FORMAT_UYVY:
      return "UYVY";
    case H264InputFormat::VIDEO_PIXEL_FORMAT_BGR888:
      return "BGR888";
    case H264InputFormat::VIDEO_PIXEL_FORMAT_RGB565_LE:
      return "RGB565_LE";
  }
  return "UNKNOWN";
}

static auto to_esp_h264_input_format(H264InputFormat format) {
  switch (format) {
    case H264InputFormat::VIDEO_PIXEL_FORMAT_O_UYY_E_VYY:
      return ESP_H264_RAW_FMT_O_UYY_E_VYY;
    case H264InputFormat::VIDEO_PIXEL_FORMAT_VUY:
      return ESP_H264_RAW_FMT_VUY;
    case H264InputFormat::VIDEO_PIXEL_FORMAT_UYVY:
      return ESP_H264_RAW_FMT_UYVY;
    case H264InputFormat::VIDEO_PIXEL_FORMAT_BGR888:
      return ESP_H264_RAW_FMT_BGR888;
    case H264InputFormat::VIDEO_PIXEL_FORMAT_RGB565_LE:
      return ESP_H264_RAW_FMT_RGB565_LE;
  }
  return ESP_H264_RAW_FMT_O_UYY_E_VYY;
}

static bool is_esp_h264_compatible(const camera_video::CameraVideoFrameSpec &spec) {
  return spec.is_complete() && spec.stride == spec.packed_row_bytes();
}

CameraH264Encoder::~CameraH264Encoder() {
  if (this->video_source_ != nullptr) {
    this->video_source_->stop_stream(this);
  }
  this->request_encoder_task_stop_();
  (void) this->finish_encoder_task_stop_(true);
  this->cleanup_encoder_resources_();
}

bool CameraH264Encoder::apply_video_source_contract_(const camera_video::CameraVideoFrameSpec &spec) {
  if (!spec.is_complete()) {
    ESP_LOGE(TAG, "video source returned an incomplete frame contract");
    return false;
  }
  if (!is_esp_h264_compatible(spec)) {
    ESP_LOGE(TAG, "video source layout is not ESP-H264 zero-copy compatible: %ux%u stride=%zu buffer=%zu", spec.width,
             spec.height, spec.stride, spec.buffer_size);
    return false;
  }

  this->width_ = spec.width;
  this->height_ = spec.height;
  this->source_fps_ = spec.fps;
  if (!this->fps_configured_) {
    if (this->source_fps_ == 0) {
      ESP_LOGE(TAG, "video source did not provide FPS; configure camera_h264 fps explicitly");
      return false;
    }
    this->fps_ = this->source_fps_;
  } else if (this->source_fps_ != 0 && this->fps_ > this->source_fps_) {
    ESP_LOGE(TAG, "configured H.264 fps=%" PRIu16 " exceeds source fps=%" PRIu16, this->fps_, this->source_fps_);
    return false;
  }
  this->rate_limit_frames_ = this->fps_configured_ && this->source_fps_ != 0 && this->fps_ < this->source_fps_;
  if (!this->gop_configured_) {
    this->gop_ = this->fps_;
  }
  this->input_format_ = spec.format;
  this->input_frame_size_ = spec.stride * spec.height;
  ESP_LOGI(TAG, "Input contract: %ux%u@%" PRIu16 "fps %s stride=%zu buffer=%zu B", spec.width, spec.height, spec.fps,
           h264_input_format_name(spec.format), spec.stride, spec.buffer_size);
  return true;
}

bool CameraH264Encoder::validate_configuration_() const {
  if (this->video_source_ == nullptr) {
    ESP_LOGE(TAG, "raw-video source is not configured");
    return false;
  }
  if (this->fps_ == 0 || this->fps_ > H264_MAX_FPS) {
    ESP_LOGE(TAG, "H.264 fps must be between 1 and %" PRIu16 ", got %" PRIu16, H264_MAX_FPS, this->fps_);
    return false;
  }
  if (this->width_ == 0 || this->height_ == 0 || (this->width_ & 1U) != 0 || (this->height_ & 1U) != 0) {
    ESP_LOGE(TAG, "input dimensions must be non-zero and even, got %" PRIu16 "x%" PRIu16, this->width_, this->height_);
    return false;
  }
  if (this->gop_ == 0 || this->gop_ > H264_MAX_GOP) {
    ESP_LOGE(TAG, "H.264 gop must be between 1 and %" PRIu16 ", got %" PRIu16, H264_MAX_GOP, this->gop_);
    return false;
  }
  if (this->encoded_frame_buffer_count_ == 0 || this->encoded_frame_buffer_count_ > H264_MAX_ENCODED_FRAME_BUFFERS) {
    ESP_LOGE(TAG, "invalid encoded_frame_buffers=%u", static_cast<unsigned>(this->encoded_frame_buffer_count_));
    return false;
  }
  return true;
}

void CameraH264Encoder::setup() {
  if (this->video_source_ == nullptr) {
    ESP_LOGE(TAG, "raw-video source is not configured");
    this->mark_failed();
    return;
  }
  this->start_encoder_task_();
}

void CameraH264Encoder::loop() {
  if (this->initialization_failed_.exchange(false, std::memory_order_acq_rel)) {
    this->mark_failed(LOG_STR("encoder initialization failed"));
  }
}

bool CameraH264Encoder::initialize_encoder_(const camera_video::CameraVideoFrameSpec &spec) {
  if (this->initialized_.load(std::memory_order_acquire)) {
    return true;
  }
  if (!this->apply_video_source_contract_(spec) || !this->validate_configuration_()) {
    return false;
  }

  ESP_LOGI(TAG, "Internal DMA free: %zu  SPIRAM free: %zu",
           heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  if (!this->init_encoder_()) {
    ESP_LOGE(TAG, "encoder init failed (contiguous internal RAM?)");
    return false;
  }

  this->encoded_frame_capacity_ = this->configured_output_buffer_size_;
  if (this->encoded_frame_capacity_ == 0) {
    this->encoded_frame_capacity_ = H264_OUTPUT_BUFFER_AUTO_BYTES;
  }
  if (!this->allocate_encoded_frames_()) {
    this->cleanup_encoder_resources_();
    return false;
  }

  if (this->streaming_.load(std::memory_order_acquire) && !this->video_source_->start_stream(this)) {
    ESP_LOGE(TAG, "Raw-video source rejected H.264 stream subscription");
    this->cleanup_encoder_resources_();
    return false;
  }
  this->initialized_.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "Raw-video source contract ready");
  ESP_LOGI(TAG, "Input %" PRIu16 "x%" PRIu16 " %s from source contract", this->width_, this->height_,
           h264_input_format_name(this->input_format_));
  return true;
}

void CameraH264Encoder::on_shutdown() {
  if (this->shutdown_started_.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  this->streaming_.store(false, std::memory_order_release);
  this->initialized_.store(false, std::memory_order_release);
  if (this->video_source_ != nullptr) {
    this->video_source_->stop_stream(this);
  }
  this->request_encoder_task_stop_();
}

bool CameraH264Encoder::teardown() {
  if (!this->finish_encoder_task_stop_(false)) {
    return false;
  }
  this->cleanup_encoder_resources_();
  return true;
}

void CameraH264Encoder::dump_config() {
  ESP_LOGCONFIG(TAG, "H.264 Encoder: %" PRIu16 "x%" PRIu16 " %s @%" PRIu16 "fps bitrate=%" PRIu32, this->width_,
                this->height_, h264_input_format_name(this->input_format_), this->fps_, this->bitrate_);
  if (this->gop_configured_) {
    ESP_LOGCONFIG(TAG, "  gop: %" PRIu16, this->gop_);
  } else {
    ESP_LOGCONFIG(TAG, "  gop: source fps");
  }
  ESP_LOGCONFIG(TAG, "  qp: %" PRIu8 "..%" PRIu8, this->qp_min_, this->qp_max_);
  ESP_LOGCONFIG(TAG, "  encoded frame buffers: %" PRIu8, this->encoded_frame_buffer_count_);
  ESP_LOGCONFIG(TAG, "  output buffer: %zu B (%s)", this->encoded_frame_capacity_,
                this->configured_output_buffer_size_ == 0 ? "automatic" : "configured");
  const uint32_t rate_limit_drops = this->rate_limit_drops_.load(std::memory_order_relaxed);
  const uint32_t pending_frame_drops = this->pending_frame_drops_.load(std::memory_order_relaxed);
  const uint32_t encoded_slot_drops = this->encoded_slot_drops_.load(std::memory_order_relaxed);
  const uint32_t encode_failures = this->encode_failures_.load(std::memory_order_relaxed);
  const uint32_t dropped_frames = rate_limit_drops + pending_frame_drops + encoded_slot_drops + encode_failures;
  ESP_LOGCONFIG(TAG, "  counters: encoded=%" PRIu32 " dropped=%" PRIu32 " errors=%" PRIu32,
                this->encoded_frames_.load(std::memory_order_relaxed), dropped_frames, encode_failures);
  ESP_LOGCONFIG(TAG, "  output buffer failures: %" PRIu32,
                this->output_buffer_failures_.load(std::memory_order_relaxed));
}

bool CameraH264Encoder::init_encoder_() {
  esp_h264_enc_cfg_hw_t config = {};
  config.gop = static_cast<uint8_t>(this->gop_);
  config.fps = this->fps_;
  config.res.width = this->width_;
  config.res.height = this->height_;
  config.rc.bitrate = this->bitrate_;
  config.rc.qp_min = this->qp_min_;
  config.rc.qp_max = this->qp_max_;
  config.pic_type = to_esp_h264_input_format(this->input_format_);

  ESP_LOGI(TAG, "esp_h264_enc_hw_new %" PRIu16 "x%" PRIu16 " %s @%" PRIu16 "fps bitrate=%" PRIu32, this->width_,
           this->height_, h264_input_format_name(this->input_format_), this->fps_, this->bitrate_);
  if (this->qp_max_ < LOW_BITRATE_QP_MAX_WARN && this->bitrate_ <= LOW_BITRATE_WARN_BITS_PER_SEC) {
    ESP_LOGW(TAG, "qp_max=%u may prevent rate control from reaching bitrate=%" PRIu32,
             static_cast<unsigned>(this->qp_max_), this->bitrate_);
  }
  ESP_LOGI(TAG, "internal free=%zu largest=%zu", heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (esp_h264_enc_hw_new(&config, &this->enc_) != ESP_H264_ERR_OK) {
    ESP_LOGE(TAG, "esp_h264_enc_hw_new failed");
    return false;
  }
  if (esp_h264_enc_open(this->enc_) != ESP_H264_ERR_OK) {
    ESP_LOGE(TAG, "esp_h264_enc_open failed");
    esp_h264_enc_del(this->enc_);
    this->enc_ = nullptr;
    return false;
  }
  return true;
}

bool CameraH264Encoder::allocate_encoded_frames_() {
  for (size_t index = 0; index < this->encoded_frame_buffer_count_; index++) {
    if (!this->encoded_frame_slots_[index].allocate(this->encoded_frame_capacity_)) {
      this->release_encoded_frames_();
      ESP_LOGE(TAG, "encoded frame allocation failed (%u x %zu B)",
               static_cast<unsigned>(this->encoded_frame_buffer_count_), this->encoded_frame_capacity_);
      return false;
    }
  }
  ESP_LOGI(TAG, "Encoded frames: %u x %zu B SPIRAM zero-copy", static_cast<unsigned>(this->encoded_frame_buffer_count_),
           this->encoded_frame_capacity_);
  return true;
}

void CameraH264Encoder::release_encoded_frames_() {
  for (auto &frame : this->encoded_frame_slots_) {
    frame.release_storage();
  }
}

H264EncodedFrame *CameraH264Encoder::acquire_encoded_frame_() {
  for (size_t index = 0; index < this->encoded_frame_buffer_count_; index++) {
    if (this->encoded_frame_slots_[index].is_available()) {
      return &this->encoded_frame_slots_[index];
    }
  }
  return nullptr;
}

void CameraH264Encoder::start_encoder_task_() {
  this->encoder_task_stop_.store(false, std::memory_order_release);
  this->encoder_task_exited_.store(false, std::memory_order_release);
  if (!this->encoder_task_.create(CameraH264Encoder::encoder_task_entry, "h264_enc", ENCODER_TASK_STACK_SIZE, this,
                                  ENCODER_TASK_PRIORITY, false)) {
    this->encoder_task_exited_.store(true, std::memory_order_release);
    ESP_LOGE(TAG, "failed to create encoder task");
    this->mark_failed(LOG_STR("encoder task creation failed"));
  }
}

void CameraH264Encoder::notify_encoder_task_() {
  const TaskHandle_t handle = this->encoder_task_.get_handle();
  if (handle != nullptr) {
    xTaskNotifyGive(handle);
  }
}

void CameraH264Encoder::request_encoder_task_stop_() {
  this->encoder_task_stop_.store(true, std::memory_order_release);
  this->notify_encoder_task_();
}

bool CameraH264Encoder::finish_encoder_task_stop_(bool force) {
  if (!this->encoder_task_.is_created()) {
    return true;
  }
  if (!this->encoder_task_exited_.load(std::memory_order_acquire)) {
    if (!force) {
      return false;
    }
    ESP_LOGW(TAG, "forcing H.264 encoder task deletion");
    this->encoder_task_.destroy();
  }
  this->encoder_task_.deallocate();
  this->encoder_task_exited_.store(false, std::memory_order_release);
  {
    LockGuard lock(this->frame_mutex_);
    this->pending_frame_.reset();
  }
  return true;
}

void CameraH264Encoder::encoder_task_entry(void *arg) {
  auto *self = static_cast<CameraH264Encoder *>(arg);
  self->encoder_task_body_();
  self->encoder_task_exited_.store(true, std::memory_order_release);
  self->enable_loop_soon_any_context();
  vTaskSuspend(nullptr);
}

bool CameraH264Encoder::queue_video_frame_(const camera_video::FrameRef<camera_video::CameraVideoFrame> &frame) {
  if (frame == nullptr || this->encoder_task_stop_.load(std::memory_order_acquire) ||
      !this->encoder_task_.is_created()) {
    this->pending_frame_drops_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (!this->frame_mutex_.try_lock()) {
    this->pending_frame_drops_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (this->pending_frame_ != nullptr) {
    this->pending_frame_drops_.fetch_add(1, std::memory_order_relaxed);
  }
  this->pending_frame_ = frame;
  this->frame_mutex_.unlock();
  this->notify_encoder_task_();
  return true;
}

void CameraH264Encoder::encoder_task_body_() {
  while (!this->encoder_task_stop_.load(std::memory_order_acquire) &&
         !this->initialized_.load(std::memory_order_acquire)) {
    camera_video::CameraVideoFrameSpec spec;
    if (this->video_source_ == nullptr || !this->video_source_->get_video_frame_spec(&spec)) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (!this->initialize_encoder_(spec)) {
      this->initialization_failed_.store(true, std::memory_order_release);
      this->enable_loop_soon_any_context();
      return;
    }
  }

  while (!this->encoder_task_stop_.load(std::memory_order_acquire)) {
    (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (this->encoder_task_stop_.load(std::memory_order_acquire)) {
      break;
    }
    camera_video::FrameRef<camera_video::CameraVideoFrame> frame;
    {
      LockGuard lock(this->frame_mutex_);
      frame.swap(this->pending_frame_);
    }
    if (frame == nullptr) {
      continue;
    }
    uint8_t *data = frame->get_data_buffer();
    const size_t length = frame->get_data_length();
    if (data == nullptr || length == 0) {
      continue;
    }
    (void) this->encode_stream_frame_(data, length, frame->get_timestamp_us());
  }
}

bool CameraH264Encoder::should_encode_live_frame_(uint64_t timestamp_us) {
  if (!this->rate_limit_frames_) {
    return true;
  }
  if (this->reset_frame_pacing_.exchange(false, std::memory_order_acq_rel)) {
    this->next_frame_due_us_ = 0;
  }
  if (this->fps_ == 0 || timestamp_us == 0) {
    return true;
  }
  const uint64_t interval_us = MICROSECONDS_PER_SECOND / static_cast<uint64_t>(this->fps_);
  if (this->next_frame_due_us_ == 0 || timestamp_us + interval_us < this->next_frame_due_us_) {
    this->next_frame_due_us_ = timestamp_us + interval_us;
    return true;
  }
  if (timestamp_us < this->next_frame_due_us_) {
    return false;
  }
  do {
    this->next_frame_due_us_ += interval_us;
  } while (this->next_frame_due_us_ <= timestamp_us);
  return true;
}

void CameraH264Encoder::on_video_frame(const camera_video::FrameRef<camera_video::CameraVideoFrame> &frame) {
  if (!this->streaming_.load(std::memory_order_acquire) || !this->initialized_.load(std::memory_order_acquire) ||
      frame == nullptr) {
    return;
  }
  uint64_t timestamp_us = frame->get_timestamp_us();
  if (timestamp_us == 0) {
    timestamp_us = static_cast<uint64_t>(esp_timer_get_time());
  }
  if (!this->should_encode_live_frame_(timestamp_us)) {
    this->rate_limit_drops_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  (void) this->queue_video_frame_(frame);
}

bool CameraH264Encoder::encode_into_buffer_(const uint8_t *data, size_t length, uint64_t timestamp_us,
                                            uint8_t *output_data, size_t output_capacity, size_t *encoded_length,
                                            bool *keyframe) {
  if (data == nullptr || output_data == nullptr || encoded_length == nullptr || keyframe == nullptr ||
      this->enc_ == nullptr) {
    return false;
  }
  const size_t expected = this->input_frame_size_;
  if (length < expected) {
    if (!this->input_mismatch_logged_) {
      ESP_LOGE(TAG, "camera frame is too small for configured %" PRIu16 "x%" PRIu16 " %s input: %zu < %zu",
               this->width_, this->height_, h264_input_format_name(this->input_format_), length, expected);
      this->input_mismatch_logged_ = true;
    }
    return false;
  }
  this->input_mismatch_logged_ = false;
  esp_h264_enc_in_frame_t input = {};
  input.raw_data.buffer = const_cast<uint8_t *>(data);
  input.raw_data.len = static_cast<uint32_t>(expected);
  input.pts = static_cast<uint32_t>(timestamp_us / 1000ULL);
  esp_h264_enc_out_frame_t output = {};
  output.raw_data.buffer = output_data;
  output.raw_data.len = static_cast<uint32_t>(output_capacity);
  if (this->force_idr_requested_.exchange(false, std::memory_order_acq_rel)) {
    esp_h264_enc_param_hw_handle_t param = nullptr;
    esp_h264_err_t force_error = esp_h264_enc_hw_get_param_hd(this->enc_, &param);
    if (force_error == ESP_H264_ERR_OK && param != nullptr) {
      force_error = esp_h264_enc_force_idr(&param->base);
    }
    if (force_error != ESP_H264_ERR_OK) {
      ESP_LOGW(TAG, "force IDR request failed: %d", static_cast<int>(force_error));
    }
  }
  const uint32_t encode_start_us = micros();
  const esp_h264_err_t error = esp_h264_enc_process(this->enc_, &input, &output);
  const uint32_t encode_time_us = micros() - encode_start_us;
  this->stats_encode_time_us_ += encode_time_us;
  this->stats_encode_time_max_us_ = std::max(this->stats_encode_time_max_us_, encode_time_us);
  this->stats_encode_samples_++;
  if (error == ESP_H264_ERR_OVERFLOW) {
    const uint32_t failures = this->output_buffer_failures_.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (failures == 1U || failures % H264_OVERFLOW_LOG_INTERVAL == 0U) {
      ESP_LOGE(TAG, "H.264 output buffer rejected (%zu B, err=%d, count=%" PRIu32 "); increase output_buffer_size",
               output_capacity, static_cast<int>(error), failures);
    }
    return false;
  }
  if (error != ESP_H264_ERR_OK) {
    ESP_LOGW(TAG, "enc_process err=%d out_len=%" PRIu32, static_cast<int>(error), output.raw_data.len);
    return false;
  }
  if (output.length == 0 || output.length > output_capacity) {
    ESP_LOGW(TAG, "invalid encoded length=%" PRIu32, output.length);
    return false;
  }
  *encoded_length = output.length;
  *keyframe = output.frame_type == ESP_H264_FRAME_TYPE_IDR;
  return true;
}

bool CameraH264Encoder::encode_stream_frame_(const uint8_t *data, size_t length, uint64_t timestamp_us) {
  this->maybe_log_stats_(micros());

  H264EncodedFrame *frame = this->acquire_encoded_frame_();
  if (frame == nullptr) {
    this->encoded_slot_drops_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  size_t encoded_length = 0;
  bool keyframe = false;
  const bool result = this->encode_into_buffer_(data, length, timestamp_us, frame->get_data_buffer(), frame->capacity(),
                                                &encoded_length, &keyframe);
  if (!result) {
    this->encode_failures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  this->stats_encoded_max_bytes_ = std::max(this->stats_encoded_max_bytes_, encoded_length);
  this->update_h264_codec_config_(frame->get_data_buffer(), encoded_length);
  camera_video::FrameRef<camera_video::H264Frame> frame_ref = frame->commit(encoded_length, timestamp_us, keyframe);
  this->encoded_frames_.fetch_add(1, std::memory_order_relaxed);
  this->publish_encoded_frame_(frame_ref);
  return true;
}

void CameraH264Encoder::maybe_log_stats_(uint32_t now_us) {
  if (this->stats_reset_requested_.exchange(false, std::memory_order_acq_rel)) {
    this->stats_start_us_ = 0;
    this->stats_encode_time_us_ = 0;
    this->stats_encode_time_max_us_ = 0;
    this->stats_encode_samples_ = 0;
    this->stats_encoded_max_bytes_ = 0;
  }
  const uint32_t encoded = this->encoded_frames_.load(std::memory_order_relaxed);
  const uint32_t rate_limit = this->rate_limit_drops_.load(std::memory_order_relaxed);
  const uint32_t pending = this->pending_frame_drops_.load(std::memory_order_relaxed);
  const uint32_t slots = this->encoded_slot_drops_.load(std::memory_order_relaxed);
  const uint32_t errors = this->encode_failures_.load(std::memory_order_relaxed);

  if (this->stats_start_us_ == 0) {
    this->stats_start_us_ = now_us;
    this->stats_last_encoded_ = encoded;
    this->stats_last_rate_limit_drops_ = rate_limit;
    this->stats_last_pending_frame_drops_ = pending;
    this->stats_last_encoded_slot_drops_ = slots;
    this->stats_last_encode_failures_ = errors;
    return;
  }

  const uint32_t elapsed_us = now_us - this->stats_start_us_;
  if (elapsed_us < H264_STATS_LOG_INTERVAL_US) {
    return;
  }

  const uint32_t encoded_delta = encoded - this->stats_last_encoded_;
  const uint32_t rate_limit_delta = rate_limit - this->stats_last_rate_limit_drops_;
  const uint32_t pending_delta = pending - this->stats_last_pending_frame_drops_;
  const uint32_t slots_delta = slots - this->stats_last_encoded_slot_drops_;
  const uint32_t errors_delta = errors - this->stats_last_encode_failures_;
  const uint32_t dropped_delta = rate_limit_delta + pending_delta + slots_delta + errors_delta;
  uint32_t encoded_fps_tenths = 0;
  if (elapsed_us != 0) {
    encoded_fps_tenths = static_cast<uint32_t>(
        (static_cast<uint64_t>(encoded_delta) * H264_STATS_DECIMAL_SCALE * MICROSECONDS_PER_SECOND + elapsed_us / 2U) /
        elapsed_us);
  }
  uint32_t average_encode_us = 0;
  if (this->stats_encode_samples_ != 0) {
    average_encode_us = static_cast<uint32_t>(this->stats_encode_time_us_ / this->stats_encode_samples_);
  }
  const uint32_t average_encode_tenths_ms = (average_encode_us + 50U) / 100U;
  const uint32_t max_encode_tenths_ms = (this->stats_encode_time_max_us_ + 50U) / 100U;

  ESP_LOGD(TAG,
           "H.264 #%" PRIu32 " encoded=%" PRIu32 ".%" PRIu32 "fps drops=%" PRIu32 " rate=%" PRIu32 " pending=%" PRIu32
           " slots=%" PRIu32 " errors=%" PRIu32 " encode_avg=%" PRIu32 ".%" PRIu32 "ms encode_max=%" PRIu32 ".%" PRIu32
           "ms max_bytes=%zu",
           encoded, encoded_fps_tenths / H264_STATS_DECIMAL_SCALE, encoded_fps_tenths % H264_STATS_DECIMAL_SCALE,
           dropped_delta, rate_limit_delta, pending_delta, slots_delta, errors_delta,
           average_encode_tenths_ms / H264_STATS_DECIMAL_SCALE, average_encode_tenths_ms % H264_STATS_DECIMAL_SCALE,
           max_encode_tenths_ms / H264_STATS_DECIMAL_SCALE, max_encode_tenths_ms % H264_STATS_DECIMAL_SCALE,
           this->stats_encoded_max_bytes_);

  this->stats_start_us_ = now_us;
  this->stats_last_encoded_ = encoded;
  this->stats_last_rate_limit_drops_ = rate_limit;
  this->stats_last_pending_frame_drops_ = pending;
  this->stats_last_encoded_slot_drops_ = slots;
  this->stats_last_encode_failures_ = errors;
  this->stats_encode_time_us_ = 0;
  this->stats_encode_time_max_us_ = 0;
  this->stats_encode_samples_ = 0;
  this->stats_encoded_max_bytes_ = 0;
}

void CameraH264Encoder::update_h264_codec_config_(const uint8_t *data, size_t length) {
  if (this->codec_config_ready_.load(std::memory_order_acquire)) {
    return;
  }

  const uint8_t *cursor = data;
  const uint8_t *end = data + length;
  camera_video::H264AnnexBNal nal;
  while (camera_video::next_annexb_nal(&cursor, end, &nal)) {
    if (nal.type() == camera_video::H264_NAL_TYPE_SPS && nal.len <= this->codec_config_.sps.size()) {
      memcpy(this->codec_config_.sps.data(), nal.data, nal.len);
      this->codec_config_.sps_length = nal.len;
      if (nal.len >= 4) {
        this->codec_config_.profile_level_id =
            static_cast<uint32_t>(nal.data[1]) << 16U | static_cast<uint32_t>(nal.data[2]) << 8U | nal.data[3];
      }
    } else if (nal.type() == camera_video::H264_NAL_TYPE_PPS && nal.len <= this->codec_config_.pps.size()) {
      memcpy(this->codec_config_.pps.data(), nal.data, nal.len);
      this->codec_config_.pps_length = nal.len;
    }
  }

  if (this->codec_config_.is_valid()) {
    this->codec_config_ready_.store(true, std::memory_order_release);
    ESP_LOGI(TAG, "H.264 codec config ready: SPS(%zu) PPS(%zu)", this->codec_config_.sps_length,
             this->codec_config_.pps_length);
  }
}

void CameraH264Encoder::publish_encoded_frame_(const camera_video::FrameRef<camera_video::H264Frame> &frame) {
  StaticVector<camera_video::H264StreamListener *, MAX_H264_LISTENERS> listeners;
  {
    LockGuard lock(this->listeners_mutex_);
    listeners.assign(this->listeners_.begin(), this->listeners_.end());
  }
  for (auto *listener : listeners) {
    listener->on_h264_frame(frame);
  }
}

void CameraH264Encoder::cleanup_encoder_resources_() {
  this->initialized_.store(false, std::memory_order_release);
  if (this->enc_ != nullptr) {
    (void) esp_h264_enc_close(this->enc_);
    (void) esp_h264_enc_del(this->enc_);
    this->enc_ = nullptr;
  }
  this->release_encoded_frames_();
  this->encoded_frame_capacity_ = 0;
}

}  // namespace esphome::camera_h264
#endif
