#pragma once

#ifdef USE_ESP32_VARIANT_ESP32P4

#include "esphome/components/camera/buffer_impl.h"
#include "processor_automation.h"

#include "driver/ppa.h"

namespace esphome::camera_pipeline {

/// Modes for the hardware image accelerator.
enum AcceleratorMode : uint8_t { ACCELERATOR_SCALE_ROTATE_MIRROR = 0, ACCELERATOR_BLEND, ACCELERATOR_FILL };

/// Rotations supported by the hardware accelerator.
enum AcceleratorRotation : uint8_t {
  ACCELERATOR_ANGLE_0 = 0,
  ACCELERATOR_ANGLE_90,
  ACCELERATOR_ANGLE_180,
  ACCELERATOR_ANGLE_270,
};

/// Returns string name for Accelerator mode.
inline const char *to_string(AcceleratorMode mode) {
  switch (mode) {
    case ACCELERATOR_SCALE_ROTATE_MIRROR:
      return "SCALE ROTATE MIRROR";
    case ACCELERATOR_BLEND:
      return "BLEND";
    case ACCELERATOR_FILL:
      return "FILL";
  }

  return "MODE_INVALID";
}

/// Returns string name for Accelerator rotation.
inline const char *to_string(AcceleratorRotation rotation) {
  switch (rotation) {
    case ACCELERATOR_ANGLE_0:
      return "0";
    case ACCELERATOR_ANGLE_90:
      return "90";
    case ACCELERATOR_ANGLE_180:
      return "180";
    case ACCELERATOR_ANGLE_270:
      return "270";
  }

  return "ROTATION_INVALID";
}

/// Hardware-accelerated processor for image scaling, conversion, flip, and rotation.
/// Receives input from other processors and outputs a scaled image for downstream processors or outputers.
class Accelerator : public ProcessorAutomation<Accelerator> {
 public:
  Accelerator(camera::BufferImpl *output);
  /// Set the accelerator mode.
  void set_mode(AcceleratorMode mode) { this->mode_ = mode; }
  /// Set rotation angle.
  void set_rotation(AcceleratorRotation rotation) { this->rotation_ = rotation; }
  /// Set horizontal scaling factor.
  void set_scale_x(float scale_x) { scale_x_ = scale_x; }
  /// Set vertical scaling factor.
  void set_scale_y(float scale_y) { scale_y_ = scale_y; }
  /// Enable or disable horizontal flip.
  void set_flip_x(bool flip_x) { this->flip_x_ = flip_x; }
  /// Enable or disable vertical flip.
  void set_flip_y(bool flip_y) { this->flip_y_ = flip_y; }
  /// Set output pixel format.
  void set_format(camera::PixelFormat format) {
    this->format_ = format;
    this->format_set_ = true;
  }
  // ------ Processor ------
  bool configure() override;
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // -----------------------

 protected:
  ppa_operation_t to_internal_(AcceleratorMode mode);
  ppa_srm_rotation_angle_t to_internal_(AcceleratorRotation rotation);
  uint32_t to_internal_(camera::PixelFormat format);
  AcceleratorMode mode_{};
  AcceleratorRotation rotation_{};
  camera::PixelFormat format_{};
  bool format_set_{};
  bool flip_x_{};
  bool flip_y_{};
  float scale_x_{};
  float scale_y_{};
  // esp_imgfx_pixel_fmt_t to_internal_(camera::PixelFormat format);
  ppa_client_handle_t ppa_client_{};
  camera::CameraImageSpec output_spec_{};
  camera::BufferImpl *output_image_{};
};

}  // namespace esphome::camera_pipeline

#endif
