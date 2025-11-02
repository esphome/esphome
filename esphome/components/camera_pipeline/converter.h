#pragma once

#ifdef USE_CAMERA_CONVERTER

#include "esphome/components/camera/buffer_impl.h"
#include "processor_automation.h"

#include "esp_imgfx_color_convert.h"

namespace esphome::camera_pipeline {

class Converter : public ProcessorAutomation<Converter> {
 public:
  Converter(camera::PixelFormat format, camera::BufferImpl *output);
  // ------ Processor ------
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------

 protected:
  esp_imgfx_pixel_fmt_t to_internal_(camera::PixelFormat format);
  uint8_t rgb565_to_grayscale(uint16_t rgb565);

  camera::PixelFormat format_{};
  camera::CameraImageSpec output_spec_{};
  camera::BufferImpl *output_image_{};
  esp_imgfx_color_convert_cfg_t cfg_{};
  esp_imgfx_color_convert_handle_t handle_{};
};

}  // namespace esphome::camera_pipeline

#endif
