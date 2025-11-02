#pragma once

#ifdef USE_CAMERA_OVERLAYER

#include "esphome/components/camera/dynamic_buffer.h"
#include "esphome/components/display/display.h"
#include "processor_automation.h"

namespace esphome::camera_pipeline {

/// Processor that overlays graphics on camera images using ESPHome's Display Rendering Engine.
/// Can optionally create a copy to avoid modifying the original image when multiple processors share it.
class Overlayer : public ProcessorAutomation<Overlayer>, public display::Display {
 public:
  /// Set an optional buffer to copy the input image before drawing.
  void set_copy_buffer(camera::DynamicBuffer *copy_buffer) { this->copy_buffer_ = copy_buffer; }
  // ------ Processor ------
  bool configure() override;
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  // ------- Display -------
  void draw_pixel_at(int x, int y, Color color) override;
  display::DisplayType get_display_type() override { return this->display_type_; }
  int get_height_internal() override { return this->output_spec_.width; }
  int get_width_internal() override { return this->output_spec_.height; }
  void update() override {}
  // -----------------------

 protected:
  display::DisplayType to_internal_(camera::PixelFormat format);

  display::DisplayType display_type_{};
  camera::DynamicBuffer *copy_buffer_{};
  camera::Buffer *output_image_{};
  camera::CameraImageSpec output_spec_{};
};

}  // namespace esphome::camera_pipeline

#endif
