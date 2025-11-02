#pragma once

#ifdef USE_CAMERA_INPUTER

#include "esphome/components/camera/callback_manager_reentry.h"
#include "esphome/components/camera/sensor.h"
#include "processor_automation.h"

namespace esphome::camera_pipeline {

/// Root node in the camera pipeline.
/// Receives image data from a CameraSensor implementation.
class Inputer : public ProcessorAutomation<Inputer> {
 public:
  Inputer(camera::Sensor *sensor) { this->sensor_ = sensor; }
  void set_retry_limit(uint16_t retry_limit) { this->retry_limit_ = retry_limit; }
  // ------ Processor ------
  bool configure() override;
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->frame_buffer_; }
  camera::ImageFormat get_output_image_format() override { return this->sensor_->get_image_format(); }
  void release_resources() override;
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override {
    return this->acquire_frame_buffer_();
  }
  camera::ProcessorError process_compressed_image_base(camera::ImageFormat input_format,
                                                       camera::Buffer *input) override {
    return this->acquire_frame_buffer_();
  }
  // -----------------------

 protected:
  camera::ProcessorError acquire_frame_buffer_();

  uint16_t retries_{};
  uint16_t retry_limit_{};
  camera::Sensor *sensor_{};
  camera::CameraImageSpec output_spec_{};
  camera::Buffer *frame_buffer_{};
};

}  // namespace esphome::camera_pipeline

#endif
