#include "processor_base.h"
#include "esphome/core/hal.h"

namespace esphome::camera_pipeline {

const char *const TAG = "camera_pipeline";

camera::ProcessorError ProcessorBase::process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  return this->run_state_machine_(camera::ImageFormat::IMAGE_FORMAT_RAW, input_spec, input);
}

camera::ProcessorError ProcessorBase::process_compressed_image(camera::ImageFormat input_format,
                                                               camera::Buffer *input) {
  return this->run_state_machine_(input_format, nullptr, input);
}

camera::ProcessorError ProcessorBase::run_state_machine_(camera::ImageFormat input_format,
                                                         camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  if (this->state_ == STATE_NEW_IMAGE) {
    if (this->statistics_) {
      this->pre_entries_ = 0;
      this->entries_ = 0;
      this->post_entries_ = 0;
      this->pre_processing_time_ = millis();
    }

    this->state_ = STATE_PRE_AUTOMATION;
  }

  if (this->state_ == STATE_PRE_AUTOMATION) {
    if (this->statistics_)
      ++this->pre_entries_;

    if (input_spec) {
      if (!pre_process_automation(input_spec, input))
        return camera::PROCESSOR_ERROR_RETRY_FRAME;
    }

    if (this->statistics_) {
      this->pre_processing_time_ = millis() - this->pre_processing_time_;
      this->processing_time_ = millis();
    }

    if (this->run_as_job_)
      this->state_ = STATE_START_JOB_TASK;
    else
      this->state_ = STATE_MAIN_TASK_PROCESSING;
  }

  if (this->state_ == STATE_MAIN_TASK_PROCESSING) {
    if (this->statistics_)
      ++this->entries_;

    if (input_format == camera::ImageFormat::IMAGE_FORMAT_RAW)
      this->error_ = this->process_pixels_base(input_spec, input);
    else
      this->error_ = this->process_compressed_image_base(input_format, input);

    if (this->error_ == camera::PROCESSOR_ERROR_RETRY_FRAME)
      return camera::PROCESSOR_ERROR_RETRY_FRAME;

    this->state_ = STATE_HANDLE_RESULT;
  }

  if (this->state_ == STATE_START_JOB_TASK) {
    bool started = task_->start([this, input_spec, input_format, input]() {
      if (input_format == camera::ImageFormat::IMAGE_FORMAT_RAW)
        this->error_ = this->process_pixels_base(input_spec, input);
      else
        this->error_ = this->process_compressed_image_base(input_format, input);
    });

    // No recover from this.
    if (!started)
      return camera::PROCESSOR_ERROR_CONFIGURATION;

    this->state_ = STATE_JOB_TASK_PROCESSING;
  }

  if (this->state_ == STATE_JOB_TASK_PROCESSING) {
    if (this->statistics_)
      ++this->entries_;

    if (this->task_->is_running())
      return camera::PROCESSOR_ERROR_RETRY_FRAME;

    this->state_ = STATE_HANDLE_RESULT;
  }

  if (this->state_ == STATE_HANDLE_RESULT) {
    switch (this->error_) {
      case camera::PROCESSOR_ERROR_SUCCESS:
        break;
      case camera::PROCESSOR_ERROR_SKIP_FRAME:
      case camera::PROCESSOR_ERROR_RETRY_FRAME:
        this->state_ = STATE_NEW_IMAGE;
      case camera::PROCESSOR_ERROR_CONFIGURATION:
        return this->error_;
    }

    if (this->statistics_) {
      this->processing_time_ = millis() - this->processing_time_;
      this->post_processing_time_ = millis();
    }
    this->state_ = STATE_POST_AUTOMATION;
  }

  if (this->state_ == STATE_POST_AUTOMATION) {
    if (this->statistics_)
      ++this->post_entries_;

    if (!post_process_automation())
      return camera::PROCESSOR_ERROR_RETRY_FRAME;

    this->state_ = STATE_NEW_IMAGE;
    if (this->statistics_) {
      this->post_processing_time_ = millis() - this->post_processing_time_;
      ESP_LOGI(TAG, "P=(%2u,%2u,%2u) ms, E=(%2u,%2u,%2u), S=%zu bytes, ID=%s", this->pre_processing_time_,
               this->processing_time_, this->post_processing_time_, this->pre_entries_, this->entries_,
               this->post_entries_, this->get_output_image()->get_size(), this->get_id());
    }
  }

  return camera::PROCESSOR_ERROR_SUCCESS;
}

}  // namespace esphome::camera_pipeline
