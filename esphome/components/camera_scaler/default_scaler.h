#pragma once

#include "esphome/components/camera/processor.h"
#include "esphome/core/color.h"

namespace esphome {
namespace camera_scaler {

/** Enumeration of different scaling algorithms.
 */
enum DefaultAlgorithm : uint8_t { NEAREST_NEIGHBOR = 0, BILINEAR };

class DefaultScaler : public camera::Processor {
 public:
  DefaultScaler(DefaultAlgorithm algorithm, camera::CameraImageSpec *spec, camera::Buffer *output);
  void set_flip_x(bool flip) { this->flip_x_ = flip; }
  void set_flip_y(bool flip) { this->flip_y_ = flip; }
  void set_clear(bool clear) { this->clear_ = clear; }
  void set_margin_left(uint16_t margin) { this->margin_left_ = margin; }
  void set_margin_right(uint16_t margin) { this->margin_right_ = margin; }
  void set_margin_top(uint16_t margin) { this->margin_top_ = margin; }
  void set_margin_bottom(uint16_t margin) { this->margin_bottom_ = margin; }
  // -------- Scaler --------
  size_t process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  camera::CameraImageSpec *get_output_image_spec() override { return this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_image_; }
  // ------------------------
 protected:
  // Grayscale methods
  uint8_t get_pixel_grayscale_nearest_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x, float y);
  uint8_t get_pixel_grayscale_bilinear_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x, float y);
  void set_pixel_grayscale_(uint8_t pixel, uint16_t x, uint16_t y);

  // RGB565 methods
  uint16_t get_pixel_rgb565_nearest_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x, float y);
  uint16_t get_pixel_rgb565_bilinear_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x, float y);
  void set_pixel_rgb565_(uint16_t pixel, uint16_t x, uint16_t y);

  // BGR888 methods
  Color get_pixel_bgr888_nearest_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x, float y);
  Color get_pixel_bgr888_bilinear_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x, float y);
  void set_pixel_bgr888_(const Color &pixel, uint16_t x, uint16_t y);

  uint16_t margin_left_{};
  uint16_t margin_right_{};
  uint16_t margin_top_{};
  uint16_t margin_bottom_{};

  DefaultAlgorithm algorithm_{};
  bool flip_x_{};
  bool flip_y_{};
  bool clear_{};
  camera::CameraImageSpec *output_spec_{};
  camera::Buffer *output_image_{};
};

}  // namespace camera_scaler
}  // namespace esphome
