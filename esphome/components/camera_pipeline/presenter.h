#pragma once

#include "processor_automation.h"

namespace esphome::camera_pipeline {

class Presenter : public ProcessorAutomation<Presenter> {
 public:
  Presenter(camera::CameraImageSpec spec, camera::Buffer *output, uint16_t x, uint16_t y);
  // ------ Processor ------
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------

 protected:
  uint16_t x_;
  uint16_t y_;
  camera::CameraImageSpec output_spec_{};
  camera::Buffer *output_image_{};
};

}  // namespace esphome::camera_pipeline
