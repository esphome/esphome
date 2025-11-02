#include "camera_impl.h"
#include "camera_image_impl.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::camera {

static const char *const TAG = "camera";

void CameraImpl::request_image(CameraRequester requester) {
  LockGuard scoped(this->lock_);

  this->next_requesters_.add(requester);
}

void CameraImpl::start_stream(CameraRequester requester) {
  LockGuard scoped(this->lock_);

  if (this->stream_requesters_.has(requester))
    return;

  this->stream_requesters_.add(requester);
  this->stream_started_ = true;
}

void CameraImpl::stop_stream(CameraRequester requester) {
  LockGuard scoped(this->lock_);

  this->stream_requesters_.clear(requester);
  this->stream_stoped_ = true;
}

void CameraImpl::setup() {
  if (!this->pipeline_->configure()) {
    this->mark_failed();
    return;
  }

  if (!this->task_->init())
    this->mark_failed("Insufficient resources to initialize Task.");
}

void CameraImpl::loop() {
  uint32_t now = App.get_loop_component_start_time();
  if (this->statistics_)
    ++this->entries_;

  // ESP32 CameraWebServer runs in own task.
  {
    LockGuard scoped(this->lock_);
    if (this->stream_started_)
      this->stream_start_callback_.call();

    if (this->stream_stoped_)
      this->stream_stop_callback_.call();

    this->stream_started_ = false;
    this->stream_stoped_ = false;
  }

  if (!this->is_publishing_ && !this->send_queue_.empty()) {
    std::shared_ptr<CameraImageImpl> image = this->send_queue_.front();
    this->send_queue_.pop();
    this->is_publishing_ = true;
    this->new_image_callback_.call(image);
    now = millis();
  }

  if (state_ == CAMERA_STATE_WAIT_FOR_REQUEST) {
    if (this->statistics_)
      this->entries_ = 1;

    // Request idle image every idle_update_interval
    if (this->idle_update_interval_ != 0 && now - this->last_idle_request_ > this->idle_update_interval_) {
      this->last_idle_request_ = now;
      this->request_image(camera::IDLE);
    }

    LockGuard scoped(this->lock_);

    this->current_requesters_ = next_requesters_ | stream_requesters_;
    next_requesters_.clear();
    if (!this->current_requesters_)
      return;

    state_ = CAMERA_STATE_PROCESSING;
    this->processing_time_ = now;
  }

  if (state_ == CAMERA_STATE_PROCESSING) {
    switch (this->pipeline_->process()) {
      case PIPELINE_ERROR_SUCCESS: {
      } break;
      case PIPELINE_ERROR_SKIP_FRAME: {
        this->state_ = CAMERA_STATE_CLEAR_REQUEST;
        return;
      } break;
      case PIPELINE_ERROR_REENTER: {
        return;
      } break;
      case PIPELINE_ERROR_CONFIGURATION: {
        this->mark_failed();
        return;
      } break;
    }

    now = millis();
    this->processing_time_ = now - processing_time_;
    this->limiter_time_ = now;
    state_ = CAMERA_STATE_RATE_LIMITING;
  }

  if (state_ == CAMERA_STATE_RATE_LIMITING) {
    if (now < this->next_update_)
      return;

    this->next_update_ = now + this->max_update_interval_;
    this->limiter_time_ = now - this->limiter_time_;
    this->outputs_ = this->pipeline_->filter_outputs(this->current_requesters_);
    this->current_output_ = this->outputs_.begin();
    if (this->outputs_.empty()) {
      this->mark_failed();
      return;
    }

    this->state_ = CAMERA_STATE_PUBLISHING;
  }

  if (this->state_ == CAMERA_STATE_PUBLISHING) {
    while (this->current_output_ != this->outputs_.end()) {
      std::shared_ptr<CameraImageImpl> image = (*current_output_)->get_image();
      if (!image)
        return;

      RequesterFlags requesters = this->pipeline_->filter_requesters(*this->current_output_, this->current_requesters_);
      this->current_requesters_ -= requesters;
      image->set_requesters(requesters);
      // Update only stats for first output.
      if (this->current_output_ == this->outputs_.begin()) {
        image->on_delete([this](CameraImageImpl *camera_image) {
          this->is_publishing_ = false;
          uint32_t now = millis();
          this->timing_fps_ = now - this->last_update_;
          this->last_update_ = now;
        });
      } else {
        image->on_delete([this](CameraImageImpl *camera_image) { this->is_publishing_ = false; });
      }

      if (this->is_publishing_) {
        this->send_queue_.push(image);
      } else {
        this->is_publishing_ = true;
        this->new_image_callback_.call(image);
      }

      ++this->current_output_;
    }

    state_ = CAMERA_STATE_CLEAR_REQUEST;
  }

  if (state_ == CAMERA_STATE_CLEAR_REQUEST) {
    state_ = CAMERA_STATE_WAIT_FOR_REQUEST;
    if (!this->statistics_)
      return;

    float fps = 0.0f;
    if (this->timing_fps_ != 0)
      fps = 1000.0f / this->timing_fps_;

    ESP_LOGI(TAG, "%.1f fps, P=%u ms, L=%u ms, E=%u", fps, this->processing_time_, this->limiter_time_, this->entries_);
  }
}

void CameraImpl::dump_config() {
  if (is_failed())
    return;

  ESP_LOGCONFIG(TAG,
                "Camera:\n"
                "  Name: %s\n"
                "  Idle update: %d ms\n"
                "  Max update: %d ms\n",
                this->name_.c_str(), idle_update_interval_, max_update_interval_);
  if (this->task_)
    this->task_->log_config(TAG);

  this->pipeline_->log_config();
}

}  // namespace esphome::camera
