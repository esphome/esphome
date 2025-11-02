#pragma once

#ifdef USE_CAMERA_ROTATOR

#include "esphome/components/camera/buffer_impl.h"
#include "processor_automation.h"

#include "esp_imgfx_rotate.h"

namespace esphome::camera_pipeline {

class Rotator : public ProcessorAutomation<Rotator> {
 public:
  Rotator(camera::BufferImpl *output, uint16_t rotation);
  void set_clear(bool clear) { this->clear_ = clear; }
  // ------ Processor ------
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------

 protected:
  esp_imgfx_pixel_fmt_t to_internal_(camera::PixelFormat format);

  camera::CameraImageSpec output_spec_{};
  camera::BufferImpl *output_image_{};
  uint16_t rotation_{};
  bool clear_{};
  esp_imgfx_rotate_cfg_t cfg_{};
  esp_imgfx_rotate_handle_t handle_{};
};

}  // namespace esphome::camera_pipeline

#endif
