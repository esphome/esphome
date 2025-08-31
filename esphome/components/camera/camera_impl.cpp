#include "camera_impl.h"
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
  this->jpeg_ = std::make_shared<CameraImageAdapter>(encoder_->get_output_buffer());
  if (this->jpeg_ == nullptr) {
    this->status_set_error("Missing set_encoder_buffer() call detected.");
    this->mark_failed();
    return;
  }

  if (this->sensor_)
    this->sensor_->camera_sensor_setup();
}

bool CameraImpl::camera_loop() {
  const uint32_t now = App.get_loop_component_start_time();

  if (state_ == CAMERA_STATE_INIT) {
    if (!this->sensor_ || !this->jpeg_) {
      this->status_set_error(
          "Missing setup() call detected. Are you overriding setup() without calling CameraImpl::setup() ?");
      this->mark_failed();
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
  }

  if (state_ == CAMERA_STATE_CAPTURE_BEGIN) {
    this->camera_incremental_context_.reset();
    state_ = CAMERA_STATE_CAPTURING;
  }

  if (state_ == CAMERA_STATE_CAPTURING) {
    this->camera_incremental_context_.done = true;
    this->image_capture_callback_.call(*this->sensor_->get_image_buffer(), *this->sensor_->get_image_spec(),
                                       this->camera_incremental_context_);
    // Incremental image capture
    if (!this->camera_incremental_context_.done)
      return true;

    // Check that we have a valid image for the encoder
    if (this->sensor_->get_image_spec()->bytes_per_image() != this->sensor_->get_image_buffer()->get_data_length()) {
      ESP_LOGE(TAG, "Spec bytes %d != %d image bytes!", this->sensor_->get_image_spec()->bytes_per_image(),
               this->sensor_->get_image_buffer()->get_data_length());
      state_ = CAMERA_STATE_CLEAR_REQUEST;
    } else {
      state_ = CAMERA_STATE_PROCESSING;
      this->input_image_ = this->sensor_->get_image_buffer();
      this->input_image_spec_ = this->sensor_->get_image_spec();
    }
  }

  if (state_ == CAMERA_STATE_PROCESSING) {
    for (Processor *processor : this->processors_) {
      processor->process_pixels(this->input_image_spec_, this->input_image_);
      this->input_image_ = processor->get_output_image();
      this->input_image_spec_ = processor->get_output_image_spec();
    }

    state_ = CAMERA_STATE_OVERLAY_BEGIN;
  }

  if (state_ == CAMERA_STATE_OVERLAY_BEGIN) {
    this->camera_incremental_context_.reset();
    state_ = CAMERA_STATE_OVERLAYING;
  }

  if (state_ == CAMERA_STATE_OVERLAYING) {
    this->camera_incremental_context_.done = true;
    this->overlay_callback_.call(*this->input_image_, *this->input_image_spec_, this->camera_incremental_context_);
    // Incremental image overlay
    if (!this->camera_incremental_context_.done)
      return true;

    state_ = CAMERA_STATE_ENCODE_BEGIN;
  }

  if (state_ == CAMERA_STATE_ENCODE_BEGIN) {
    // Is image still in publishing ?
    if (this->jpeg_.use_count() > 1) {
      return false;
    }

    state_ = CAMERA_STATE_ENCODING;
  }

  if (state_ == CAMERA_STATE_ENCODING) {
    // Encodes the pixels and returns the number of bytes written.
    EncoderError error = this->encoder_->encode_pixels(this->input_image_spec_, this->input_image_);
    switch (error) {
      case ENCODER_ERROR_SUCCESS: {
        if (skip_frame_counter_ > 1)
          ESP_LOGW(TAG, "ENCODER_ERROR_SKIP_FRAME. TOTAL: %d", skip_frame_counter_);

        if (retry_frame_counter_ > 1)
          ESP_LOGV(TAG, "ENCODER_ERROR_RETRY_FRAME. TOTAL: %d", retry_frame_counter_);

        skip_frame_counter_ = 0;
        retry_frame_counter_ = 0;
        state_ = CAMERA_STATE_RATE_LIMITING;
      } break;
        break;
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
        this->status_set_error("ENCODER_ERROR_CONFIGURATION");
        this->mark_failed();
        return false;
      } break;
    }
  }

  if (state_ == CAMERA_STATE_RATE_LIMITING) {
    if (now - this->last_update_ <= this->max_update_interval_)
      return false;

    state_ = CAMERA_STATE_PUBLISHING;
  }

  if (state_ == CAMERA_STATE_PUBLISHING) {
    this->jpeg_->set_requesters(this->image_requesters_ | this->stream_requesters_);
    this->new_image_callback_.call(this->jpeg_);
    this->last_update_ = now;
    state_ = CAMERA_STATE_CLEAR_REQUEST;
  }

  if (state_ == CAMERA_STATE_CLEAR_REQUEST) {
    this->image_requesters_ = 0;
    state_ = CAMERA_STATE_WAIT_FOR_REQUEST;
  }

  return false;
}

void CameraImpl::loop() { camera_loop(); }

void CameraImpl::dump_config() {
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
    this->sensor_->camera_sensor_dump_config();
  if (this->encoder_)
    this->encoder_->dump_config();
}

}  // namespace camera
}  // namespace esphome
