#pragma once

#ifdef USE_CAMERA_SCALER

#include "esphome/components/camera/buffer_impl.h"
#include "processor_automation.h"

#include "esp_imgfx_scale.h"

namespace esphome::camera_pipeline {

/// Enumeration of different scaling algorithms.
enum ScalerAlgorithm : uint8_t { DOWN_RESAMPLE = 0, BILINEAR };

/// Returns string name for scaler algorithm.
inline const char *to_string(ScalerAlgorithm algorithm) {
  switch (algorithm) {
    case DOWN_RESAMPLE:
      return "DOWN_RESAMPLE";
    case BILINEAR:
      return "BILINEAR";
  }

  return "SCALER_ALGORITHM_INVALID";
}

class Scaler : public ProcessorAutomation<Scaler> {
 public:
  Scaler(ScalerAlgorithm algorithm, uint16_t width, uint16_t height, camera::BufferImpl *output);
  // ------ Processor ------
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------

 protected:
  esp_imgfx_scale_filter_type_t to_internal_(ScalerAlgorithm algorithm);
  esp_imgfx_pixel_fmt_t to_internal_(camera::PixelFormat format);

  ScalerAlgorithm algorithm_{};
  camera::CameraImageSpec output_spec_{};
  camera::BufferImpl *output_image_{};
  esp_imgfx_scale_cfg_t cfg_{};
  esp_imgfx_scale_handle_t handle_{};
};

}  // namespace esphome::camera_pipeline

#endif
