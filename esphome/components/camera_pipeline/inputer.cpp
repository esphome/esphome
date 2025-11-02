#ifdef USE_CAMERA_INPUTER

#include "inputer.h"
#include "esphome/core/log.h"

namespace esphome::camera_pipeline {

bool Inputer::configure() {
  if (!this->sensor_) {
    ESP_LOGE(TAG, "No Camera Sensor!");
    return false;
  }

  if (!this->sensor_->configure()) {
    ESP_LOGE(TAG, "Camera Sensor configure failed!");
    return false;
  }

  camera::Resolution resolution = this->sensor_->get_resolution();
  this->output_spec_.width = resolution.width;
  this->output_spec_.height = resolution.height;

  // Compressed formats, JPEG etc...
  if (this->sensor_->get_image_format() != camera::IMAGE_FORMAT_RAW)
    return true;

  // RGB formats
  std::optional<camera::PixelFormat> format = this->sensor_->get_pixel_format();
  if (!format.has_value()) {
    ESP_LOGE(TAG, "Camera sensor provides raw data without pixel format!");
    return false;
  }

  this->output_spec_.format = format.value();
  return true;
}

void Inputer::release_resources() { this->sensor_->return_frame_buffer(this->frame_buffer_); }

void Inputer::log_config() {
  ESP_LOGCONFIG(TAG,
                "Inputer: %s\n"
                "  %s\n"
                "  Retry Limit: %u\n"
                "  Job: %s\n",
                this->get_id(), to_string(this->sensor_->get_image_format()), this->retry_limit_,
                YESNO(this->run_as_job_));

  this->sensor_->log_config();
}

camera::ProcessorError Inputer::acquire_frame_buffer_() {
  this->frame_buffer_ = this->sensor_->acquire_frame_buffer();
  if (this->frame_buffer_) {
    this->retries_ = 0;
    return camera::PROCESSOR_ERROR_SUCCESS;
  }

  if (this->retry_limit_ == 0)
    return camera::PROCESSOR_ERROR_RETRY_FRAME;

  ++this->retries_;
  if (this->retries_ >= this->retry_limit_) {
    ESP_LOGE(TAG, "No frame received from camera sensor after %u attempts! 'esp_ldo' defined ?", this->retries_);
    return camera::PROCESSOR_ERROR_CONFIGURATION;
  }

  return camera::PROCESSOR_ERROR_RETRY_FRAME;
}

}  // namespace esphome::camera_pipeline

#endif
