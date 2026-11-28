#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstdint>
#include <memory>

#include "esphome/components/camera/camera.h"
#include "esphome/components/camera/decoder.h"
#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"
#include "esphome/core/component.h"

namespace esphome::tflite_camera {

/// Decodes frames from a camera and runs the image model on them.
class TFLiteCamera final : public Component, public camera::CameraListener {
 public:
  explicit TFLiteCamera(tflite_micro_runtime::TFLiteMicroHelper *runtime) : runtime_(runtime) {}

  void set_camera(camera::Camera *camera) { this->camera_ = camera; }
  void set_decoder(camera::Decoder *decoder) { this->decoder_ = decoder; }
  /// Minimum time between two inferences; a frame that arrives sooner is dropped.
  void set_min_interval(uint32_t ms) { this->min_interval_ms_ = ms; }

  void setup() override;
  void dump_config() override;
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;

 protected:
  tflite_micro_runtime::TFLiteMicroHelper *runtime_;
  camera::Camera *camera_{nullptr};
  camera::Decoder *decoder_{nullptr};
  uint32_t min_interval_ms_{0};
  uint32_t last_run_ms_{0};
  bool has_run_{false};
};

}  // namespace esphome::tflite_camera

#endif  // USE_TFLITE_MICRO_HELPER
