#include "default_scaler.h"

namespace esphome {
namespace camera_scaler {

DefaultScaler::DefaultScaler(DefaultAlgorithm algorithm, camera::CameraImageSpec *spec, camera::Buffer *output) {
  this->algorithm_ = algorithm;
  this->output_spec_ = spec;
  this->output_image_ = output;
}

size_t DefaultScaler::process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  if (clear_)
    memset(this->output_image_->get_data_buffer(), 0, this->output_spec_->bytes_per_image());

  uint16_t dst_width = this->output_spec_->width - this->margin_left_ - this->margin_right_;
  uint16_t dst_height = this->output_spec_->height - this->margin_top_ - this->margin_bottom_;

  float src_y_start = 0.0f;
  float src_x_start = 0.0f;
  float src_dy_step = 0.0f;
  float src_dx_step = 0.0f;

  switch (algorithm_) {
    case NEAREST_NEIGHBOR: {
      src_dy_step = (float) (input_spec->height) / (float) dst_height;
      src_dx_step = (float) (input_spec->width) / (float) dst_width;
      src_y_start = 0.5f * src_dy_step;
      src_x_start = 0.5f * src_dx_step;
    } break;
    case BILINEAR: {
      src_dy_step = (input_spec->height - 1.0f) / (dst_height - 1.0f);
      src_dx_step = (input_spec->width - 1.0f) / (dst_width - 1.0f);
    } break;
  }

  float src_y = src_y_start;
  for (uint16_t y = 0; y < dst_height; ++y) {
    float src_x = src_x_start;
    for (uint16_t x = 0; x < dst_width; ++x) {
      if (input_spec->format == camera::IMAGE_FORMAT_GRAYSCALE) {
        uint8_t pixel = 0;
        switch (algorithm_) {
          case NEAREST_NEIGHBOR: {
            pixel = get_pixel_grayscale_nearest_(input_spec, input, src_x, src_y);
          } break;
          case BILINEAR: {
            pixel = get_pixel_grayscale_bilinear_(input_spec, input, src_x, src_y);
          } break;
        }
        this->set_pixel_grayscale_(pixel, (this->flip_x_ ? (dst_width - 1 - x) : x),
                                   this->margin_top_ + (this->flip_y_ ? (dst_height - 1 - y) : y));
      }
      src_x += src_dx_step;
    }
    src_y += src_dy_step;
  }

  return 0;
}

uint8_t DefaultScaler::get_pixel_grayscale_nearest_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x,
                                                    float y) {
  uint16_t x0 = x;
  uint16_t y0 = y;
  return input->get_data_buffer()[y0 * input_spec->width + x0];
}

uint8_t DefaultScaler::get_pixel_grayscale_bilinear_(camera::CameraImageSpec *input_spec, camera::Buffer *input,
                                                     float x, float y) {
  uint16_t x0 = x;
  uint16_t y0 = y;
  uint16_t x1 = x + 1;
  uint16_t y1 = y + 1;

  float dx = x - x0;
  float dy = y - y0;

  if (x1 >= input_spec->width)
    x1 = x;

  if (y1 >= input_spec->height)
    y1 = y;

  uint16_t idxx = y0 * input_spec->width;
  uint16_t idx00 = idxx + x0;
  uint16_t idx01 = idxx + x1;
  uint16_t idxy = y1 * input_spec->width;
  uint16_t idx10 = idxy + x0;
  uint16_t idx11 = idxy + x1;

  uint8_t *buffer = input->get_data_buffer();
  float p00 = buffer[idx00];
  float p01 = buffer[idx01];
  float p10 = buffer[idx10];
  float p11 = buffer[idx11];

  float py0 = p00 * (1.0f - dx) + p01 * dx;
  float py1 = p10 * (1.0f - dx) + p11 * dx;

  return static_cast<uint8_t>(lroundf(py0 * (1.0f - dy) + py1 * dy));
}

void DefaultScaler::set_pixel_grayscale_(uint8_t pixel, uint16_t x, uint16_t y) {
  uint16_t idx = (y * this->output_spec_->width + x);
  this->output_image_->get_data_buffer()[idx] = pixel;
}

}  // namespace camera_scaler
}  // namespace esphome
