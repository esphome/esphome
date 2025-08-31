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
      uint16_t dst_x = this->margin_left_ + (this->flip_x_ ? (dst_width - 1 - x) : x);
      uint16_t dst_y = this->margin_top_ + (this->flip_y_ ? (dst_height - 1 - y) : y);

      switch (input_spec->format) {
        case camera::PIXEL_FORMAT_GRAYSCALE: {
          uint8_t pixel = 0;
          switch (algorithm_) {
            case NEAREST_NEIGHBOR:
              pixel = get_pixel_grayscale_nearest_(input_spec, input, src_x, src_y);
              break;
            case BILINEAR:
              pixel = get_pixel_grayscale_bilinear_(input_spec, input, src_x, src_y);
              break;
          }
          this->set_pixel_grayscale_(pixel, dst_x, dst_y);
        } break;

        case camera::PIXEL_FORMAT_RGB565: {
          uint16_t pixel = 0;
          switch (algorithm_) {
            case NEAREST_NEIGHBOR:
              pixel = get_pixel_rgb565_nearest_(input_spec, input, src_x, src_y);
              break;
            case BILINEAR:
              pixel = get_pixel_rgb565_bilinear_(input_spec, input, src_x, src_y);
              break;
          }
          this->set_pixel_rgb565_(pixel, dst_x, dst_y);
        } break;

        case camera::PIXEL_FORMAT_BGR888: {
          Color pixel;
          switch (algorithm_) {
            case NEAREST_NEIGHBOR:
              pixel = get_pixel_bgr888_nearest_(input_spec, input, src_x, src_y);
              break;
            case BILINEAR:
              pixel = get_pixel_bgr888_bilinear_(input_spec, input, src_x, src_y);
              break;
          }
          this->set_pixel_bgr888_(pixel, dst_x, dst_y);
        } break;

        default:
          break;
      }
      src_x += src_dx_step;
    }
    src_y += src_dy_step;
  }

  return 0;
}

// Grayscale methods
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

// RGB565 methods
uint16_t DefaultScaler::get_pixel_rgb565_nearest_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x,
                                                  float y) {
  uint16_t x0 = x;
  uint16_t y0 = y;
  uint16_t *buffer = reinterpret_cast<uint16_t *>(input->get_data_buffer());
  return buffer[y0 * input_spec->width + x0];
}

uint16_t DefaultScaler::get_pixel_rgb565_bilinear_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x,
                                                   float y) {
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

  uint16_t *buffer = reinterpret_cast<uint16_t *>(input->get_data_buffer());
  uint16_t idx00 = y0 * input_spec->width + x0;
  uint16_t idx01 = y0 * input_spec->width + x1;
  uint16_t idx10 = y1 * input_spec->width + x0;
  uint16_t idx11 = y1 * input_spec->width + x1;

  // Extract RGB components from RGB565
  auto extract_rgb = [](uint16_t pixel) -> std::tuple<float, float, float> {
    float r = ((pixel >> 11) & 0x1F) * 255.0f / 31.0f;
    float g = ((pixel >> 5) & 0x3F) * 255.0f / 63.0f;
    float b = (pixel & 0x1F) * 255.0f / 31.0f;
    return {r, g, b};
  };

  auto [r00, g00, b00] = extract_rgb(buffer[idx00]);
  auto [r01, g01, b01] = extract_rgb(buffer[idx01]);
  auto [r10, g10, b10] = extract_rgb(buffer[idx10]);
  auto [r11, g11, b11] = extract_rgb(buffer[idx11]);

  // Interpolate each component
  float r0 = r00 * (1.0f - dx) + r01 * dx;
  float g0 = g00 * (1.0f - dx) + g01 * dx;
  float b0 = b00 * (1.0f - dx) + b01 * dx;

  float r1 = r10 * (1.0f - dx) + r11 * dx;
  float g1 = g10 * (1.0f - dx) + g11 * dx;
  float b1 = b10 * (1.0f - dx) + b11 * dx;

  float r = r0 * (1.0f - dy) + r1 * dy;
  float g = g0 * (1.0f - dy) + g1 * dy;
  float b = b0 * (1.0f - dy) + b1 * dy;

  // Convert back to RGB565
  uint16_t r5 = static_cast<uint16_t>(r * 31.0f / 255.0f) & 0x1F;
  uint16_t g6 = static_cast<uint16_t>(g * 63.0f / 255.0f) & 0x3F;
  uint16_t b5 = static_cast<uint16_t>(b * 31.0f / 255.0f) & 0x1F;

  return (r5 << 11) | (g6 << 5) | b5;
}

void DefaultScaler::set_pixel_rgb565_(uint16_t pixel, uint16_t x, uint16_t y) {
  uint16_t *buffer = reinterpret_cast<uint16_t *>(this->output_image_->get_data_buffer());
  uint16_t idx = (y * this->output_spec_->width + x);
  buffer[idx] = pixel;
}

// BGR888 methods
Color DefaultScaler::get_pixel_bgr888_nearest_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x,
                                               float y) {
  uint16_t x0 = x;
  uint16_t y0 = y;
  uint8_t *buffer = input->get_data_buffer();
  uint32_t idx = (y0 * input_spec->width + x0) * 3;

  return Color(buffer[idx + 2], buffer[idx + 1], buffer[idx]);  // Convert BGR to RGB
}

Color DefaultScaler::get_pixel_bgr888_bilinear_(camera::CameraImageSpec *input_spec, camera::Buffer *input, float x,
                                                float y) {
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

  uint8_t *buffer = input->get_data_buffer();

  auto get_bgr = [&](uint16_t x, uint16_t y) -> std::tuple<float, float, float> {
    uint32_t idx = (y * input_spec->width + x) * 3;
    float b = buffer[idx];
    float g = buffer[idx + 1];
    float r = buffer[idx + 2];
    return {r, g, b};  // Return as RGB
  };

  auto [r00, g00, b00] = get_bgr(x0, y0);
  auto [r01, g01, b01] = get_bgr(x1, y0);
  auto [r10, g10, b10] = get_bgr(x0, y1);
  auto [r11, g11, b11] = get_bgr(x1, y1);

  // Interpolate each component
  float r0 = r00 * (1.0f - dx) + r01 * dx;
  float g0 = g00 * (1.0f - dx) + g01 * dx;
  float b0 = b00 * (1.0f - dx) + b01 * dx;

  float r1 = r10 * (1.0f - dx) + r11 * dx;
  float g1 = g10 * (1.0f - dx) + g11 * dx;
  float b1 = b10 * (1.0f - dx) + b11 * dx;

  float r = r0 * (1.0f - dy) + r1 * dy;
  float g = g0 * (1.0f - dy) + g1 * dy;
  float b = b0 * (1.0f - dy) + b1 * dy;

  return Color(static_cast<uint8_t>(lroundf(r)), static_cast<uint8_t>(lroundf(g)), static_cast<uint8_t>(lroundf(b)));
}

void DefaultScaler::set_pixel_bgr888_(const Color &pixel, uint16_t x, uint16_t y) {
  uint8_t *buffer = this->output_image_->get_data_buffer();
  uint32_t idx = (y * this->output_spec_->width + x) * 3;

  // Convert RGB to BGR
  buffer[idx] = pixel.b;      // Blue
  buffer[idx + 1] = pixel.g;  // Green
  buffer[idx + 2] = pixel.r;  // Red
}

}  // namespace camera_scaler
}  // namespace esphome
