#pragma once

#ifdef USE_CAMERA_CROPPER

#include "esphome/components/camera/buffer_impl.h"
#include "processor_automation.h"

#include "esp_imgfx_crop.h"

namespace esphome::camera_pipeline {

class Cropper : public ProcessorAutomation<Cropper> {
 public:
  Cropper(uint16_t width, uint16_t height, camera::BufferImpl *output, int crop_x, int crop_y);
  // ------ Processor ------
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------

 protected:
  esp_imgfx_pixel_fmt_t to_internal_(camera::PixelFormat format);

  int crop_x_;
  int crop_y_;
  camera::CameraImageSpec output_spec_{};
  camera::BufferImpl *output_image_{};
  esp_imgfx_crop_cfg_t cfg_{};
  esp_imgfx_crop_handle_t handle_{};
};

}  // namespace esphome::camera_pipeline

#endif
