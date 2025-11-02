#pragma once

#ifdef USE_CAMERA_INFERENCER

#include "processor_automation.h"

#include "dl_model_base.hpp"
#include "dl_image_preprocessor.hpp"
#include "dl_image_define.hpp"
#include "dl_tensor_base.hpp"

namespace esphome::camera_pipeline {

/// Runtime modes for the inferencer processor.
enum InferencerMode : uint8_t {
  AUTO = 0,     ///< Automatically select mode.
  SINGLE_CORE,  ///< Run on single core.
  MULTI_CORE    ///< Run on multiple cores.
};

/// Returns string name for inferencer mode.
inline const char *to_string(InferencerMode mode) {
  switch (mode) {
    case AUTO:
      return "AUTO";
    case SINGLE_CORE:
      return "SINGLE_CORE";
    case MULTI_CORE:
      return "MULTI_CORE";
  }

  return "MODE_INVALID";
}

/// Processor for running neural network inference on camera images.
/// Supports automatic, single-core, and multi-core execution modes.
class Inferencer : public ProcessorAutomation<Inferencer> {
 public:
  /// Constructor with read-only model data.
  Inferencer(const uint8_t *rodata);
  /// Set runtime mode for inference.
  void set_mode(InferencerMode mode) { this->mode_ = mode; }
  // ------ Processor ------
  bool configure() override;
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------
  /// Get pointer to loaded model.
  dl::Model *get_model() { return this->model_; }
  /// Convert pixel format for inference input.
  dl::image::pix_type_t convert(camera::PixelFormat format);

 protected:
  dl::runtime_mode_t to_internal_(InferencerMode mode);

  InferencerMode mode_{};
  int height_{};
  int width_{};
  int channels_{};
  const uint8_t *rodata_;
  camera::CameraImageSpec output_spec_{};
  camera::Buffer *output_image_{};
  dl::Model *model_{};
  dl::TensorBase *input_tensor_{};
};

}  // namespace esphome::camera_pipeline

#endif
