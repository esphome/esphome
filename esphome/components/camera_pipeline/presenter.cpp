#include "presenter.h"
#include "esphome/core/log.h"

namespace esphome::camera_pipeline {

Presenter::Presenter(camera::CameraImageSpec spec, camera::Buffer *output, uint16_t x, uint16_t y) {
  this->output_spec_ = spec;
  this->output_image_ = output;
  this->x_ = x;
  this->y_ = y;
}

void Presenter::log_config() {}

camera::ProcessorError Presenter::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  if (this->output_spec_.format != input_spec->format)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  if (this->output_spec_.width <= this->x_)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  uint16_t width = this->output_spec_.width - this->x_;
  width = width < input_spec->width ? width : input_spec->width;

  uint16_t height = this->output_spec_.height - this->y_;
  height = height < input_spec->height ? height : input_spec->height;

  if (height == 0 || width == 0)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  uint8_t *src = input->get_data();
  uint8_t *dst = output_image_->get_data() + (this->y_ * output_spec_.bytes_per_row()) +
                 (this->x_ * output_spec_.bytes_per_pixel());
  for (uint16_t row = 0; row < height; ++row) {
    memcpy(dst, src, width * output_spec_.bytes_per_pixel());
    src += input_spec->bytes_per_row();
    dst += output_spec_.bytes_per_row();
  }

  this->output_spec_.format = input_spec->format;
  return camera::PROCESSOR_ERROR_SUCCESS;
}

}  // namespace esphome::camera_pipeline
