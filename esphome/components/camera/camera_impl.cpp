#include "camera_impl.h"
#include "camera_image_impl.h"
#include "camera_image_reader_impl.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

static const char *const TAG = "camera";

namespace esphome {
namespace camera {

CameraImageReader *CameraImpl::create_image_reader() {
  ESP_LOGV(TAG, "CameraImpl::create_image_reader.");
  return new CameraImageReaderImpl;
}

void CameraImpl::request_image(CameraRequester requester) {
  ESP_LOGV(TAG, "CameraImpl::request_image requester: %d", requester);
  this->image_requesters_ |= (1U << requester);
}

void CameraImpl::start_stream(CameraRequester requester) {
  ESP_LOGV(TAG, "CameraImpl::start_stream requester: %d", requester);
  if (this->stream_requesters_ & (1U << requester))
    return;

  this->stream_start_callback_.call();
  this->stream_requesters_ |= (1U << requester);
}

void CameraImpl::stop_stream(CameraRequester requester) {
  ESP_LOGV(TAG, "CameraImpl::stop_stream requester: %d", requester);
  this->stream_stop_callback_.call();
  this->stream_requesters_ &= ~(1U << requester);
}

void CameraImpl::setup() {
  if (this->sensor_ == nullptr) {
    this->mark_failed("Missing set_sensor() call detected.");
    return;
  }

  if (!this->sensor_->configure()) {
    this->mark_failed("Camera sensor configure failed!");
    return;
  }
}

bool CameraImpl::camera_loop() {
  const uint32_t now = App.get_loop_component_start_time();

  if (!this->is_publishing_ && !this->send_queue_.empty()) {
    std::shared_ptr<CameraImageImpl> image = this->send_queue_.front();
    this->send_queue_.pop();
    this->is_publishing_ = true;
    this->new_image_callback_.call(image);
  }

  if (state_ == CAMERA_STATE_INIT) {
    if (!this->sensor_) {
      this->mark_failed(
          "Missing setup() call detected. Are you overriding setup() without calling CameraImpl::setup() ?");
      return false;
    }

    state_ = CAMERA_STATE_WAIT_FOR_REQUEST;
  }

  if (state_ == CAMERA_STATE_WAIT_FOR_REQUEST) {
    // Request idle image every idle_update_interval
    if (this->idle_update_interval_ != 0 && now - this->last_idle_request_ > this->idle_update_interval_) {
      this->last_idle_request_ = now;
      this->request_image(camera::IDLE);
    }

    // No new image requested
    if (!image_requesters_ && !stream_requesters_)
      return false;

    state_ = CAMERA_STATE_CAPTURE_BEGIN;
    timing_fps_ = now;
  }

  if (state_ == CAMERA_STATE_CAPTURE_BEGIN) {
    this->camera_incremental_context_.reset();
    timing_capture_ = now;
    this->frame_buffer_ = this->sensor_->acquire_frame_buffer();
    if (!this->frame_buffer_) {
      return true;
    }

    this->input_image_ = this->frame_buffer_;
    if (this->sensor_->get_image_format() == IMAGE_FORMAT_RAW) {
      Resolution r = this->sensor_->get_resolution();
      this->input_image_spec_.width = r.width;
      this->input_image_spec_.height = r.height;
      this->input_image_spec_.format = this->sensor_->get_pixel_format().value();
      state_ = CAMERA_STATE_CAPTURING;
    } else {
      state_ = CAMERA_STATE_RATE_LIMIT_BEGIN;
    }
    timing_capture_ = millis() - timing_capture_;
    timing_capture_callback_ = 0;
  }

  if (state_ == CAMERA_STATE_CAPTURING) {
    this->camera_incremental_context_.done = true;

    uint32_t dt = millis();
    this->image_capture_callback_.call(*this->input_image_, this->input_image_spec_, this->camera_incremental_context_);
    this->timing_capture_callback_ += millis() - dt;
    // Incremental image capture
    if (!this->camera_incremental_context_.done)
      return true;

    // Check that we have a valid image for the encoder
    if (this->input_image_spec_.bytes_per_image() != this->input_image_->get_size()) {
      ESP_LOGE(TAG, "Spec bytes %d != %d image bytes!", this->input_image_spec_.bytes_per_image(),
               this->input_image_->get_size());
      this->sensor_->return_frame_buffer(this->input_image_);
      state_ = CAMERA_STATE_CLEAR_REQUEST;
    } else {
      state_ = CAMERA_STATE_PROCESSING;
    }
  }

  if (state_ == CAMERA_STATE_PROCESSING) {
    timing_processing_ = millis();
    for (Processor *processor : this->processors_) {
      processor->process_pixels(&this->input_image_spec_, this->input_image_);
      this->input_image_ = processor->get_output_image();
      this->input_image_spec_ = *processor->get_output_image_spec();
    }

    timing_processing_ = millis() - timing_processing_;
    state_ = CAMERA_STATE_OVERLAY_BEGIN;
  }

  if (state_ == CAMERA_STATE_OVERLAY_BEGIN) {
    this->camera_incremental_context_.reset();
    this->timing_overlay_ = 0;
    state_ = CAMERA_STATE_OVERLAYING;
  }

  if (state_ == CAMERA_STATE_OVERLAYING) {
    this->camera_incremental_context_.done = true;
    uint32_t dt = millis();
    this->overlay_callback_.call(*this->input_image_, this->input_image_spec_, this->camera_incremental_context_);
    this->timing_overlay_ += millis() - dt;
    // Incremental image overlay
    if (!this->camera_incremental_context_.done)
      return true;

    state_ = CAMERA_STATE_ENCODE_BEGIN;
    timing_wait_ = millis();
  }

  if (state_ == CAMERA_STATE_ENCODE_BEGIN) {
    // Wait to get the encoder output buffer back.
    if (this->is_publishing_ || !this->send_queue_.empty())
      return true;

    timing_encoding_ = millis();
    timing_wait_ = timing_encoding_ - timing_wait_;
    this->state_ = CAMERA_STATE_ENCODING;
  }

  if (state_ == CAMERA_STATE_ENCODING) {
    // Encodes the pixels and returns the number of bytes written.
    EncoderError error = this->encoder_->encode_pixels(&this->input_image_spec_, this->input_image_);
    switch (error) {
      case ENCODER_ERROR_SUCCESS: {
        if (skip_frame_counter_ > 1)
          ESP_LOGW(TAG, "ENCODER_ERROR_SKIP_FRAME. TOTAL: %d", skip_frame_counter_);

        if (retry_frame_counter_ > 1)
          ESP_LOGV(TAG, "ENCODER_ERROR_RETRY_FRAME. TOTAL: %d", retry_frame_counter_);

        skip_frame_counter_ = 0;
        retry_frame_counter_ = 0;
        this->input_image_ = this->encoder_->get_output_buffer();
        state_ = CAMERA_STATE_RATE_LIMIT_BEGIN;
        timing_encoding_ = millis() - timing_encoding_;
      } break;
      case ENCODER_ERROR_SKIP_FRAME: {
        if (skip_frame_counter_ == 0)
          ESP_LOGW(TAG, "ENCODER_ERROR_SKIP_FRAME.");

        ++skip_frame_counter_;
        state_ = CAMERA_STATE_CLEAR_REQUEST;
      } break;
      case ENCODER_ERROR_RETRY_FRAME: {
        if (retry_frame_counter_ == 0)
          ESP_LOGV(TAG, "ENCODER_ERROR_RETRY_FRAME.");

        ++retry_frame_counter_;
        return true;
      } break;
      case ENCODER_ERROR_CONFIGURATION: {
        this->mark_failed("ENCODER_ERROR_CONFIGURATION");
        return false;
      } break;
    }
  }

  if (this->state_ == CAMERA_STATE_RATE_LIMIT_BEGIN) {
    this->timing_limiter_ = millis();
    this->state_ = CAMERA_STATE_RATE_LIMITING;
  }

  if (state_ == CAMERA_STATE_RATE_LIMITING) {
    if (millis() - this->last_update_ <= this->max_update_interval_)
      return false;

    this->timing_limiter_ = millis() - this->timing_limiter_;
    this->state_ = CAMERA_STATE_PUBLISHING;
  }

  if (state_ == CAMERA_STATE_PUBLISHING) {
    std::shared_ptr<CameraImageImpl> image = std::make_shared<CameraImageImpl>();
    image->set_buffer(this->input_image_);
    image->set_requesters(this->image_requesters_ | this->stream_requesters_);
    image->on_delete([this, frame_buffer = this->frame_buffer_](CameraImageImpl *camera_image) {
      this->sensor_->return_frame_buffer(frame_buffer);
      this->is_publishing_ = false;
    });

    if (this->is_publishing_) {
      this->send_queue_.push(image);
    } else {
      this->is_publishing_ = true;
      this->new_image_callback_.call(image);
    }

    this->last_update_ = millis();
    this->timing_fps_ = this->last_update_ - this->timing_fps_;
    state_ = CAMERA_STATE_CLEAR_REQUEST;

    float fps = 0.0f;
    if (this->timing_fps_ != 0)
      fps = 1000.0f / this->timing_fps_;

    if (this->encoder_) {
      ESP_LOGD(TAG, "F=%.1f, S=%zu, C=%u ms, CC=%u ms, P=%u ms, O=%u ms, L=%u ms, W=%u ms, E=%u ms", fps,
               this->input_image_->get_size(), this->timing_capture_, this->timing_capture_callback_,
               this->timing_processing_, this->timing_overlay_, this->timing_limiter_, this->timing_wait_,
               this->timing_encoding_);
    } else {
      ESP_LOGD(TAG, "F=%.1f, S=%zu, C=%u ms, CC=%u ms, P=%u ms, O=%u ms, L=%u ms, ", fps,
               this->input_image_->get_size(), this->timing_capture_, this->timing_capture_callback_,
               this->timing_processing_, this->timing_overlay_, this->timing_limiter_);
    }
  }

  if (state_ == CAMERA_STATE_CLEAR_REQUEST) {
    this->image_requesters_ = 0;
    state_ = CAMERA_STATE_WAIT_FOR_REQUEST;
  }

  return false;
}

void CameraImpl::loop() { camera_loop(); }

void CameraImpl::dump_config() {
  if (is_failed())
    return;

  ESP_LOGCONFIG(TAG,
                "Camera:\n"
                "  Name: %s\n"
                "  Internal: %s\n"
                "  Idle update: %d ms\n"
                "  Max update: %d ms\n"
                "  Encoder enabled: %s\n",
                this->name_.c_str(), YESNO(this->is_internal()), idle_update_interval_, max_update_interval_,
                YESNO(this->encoder_));
  if (this->sensor_)
    this->sensor_->log_config();
  if (this->encoder_)
    this->encoder_->dump_config();
}

}  // namespace camera
}  // namespace esphome
