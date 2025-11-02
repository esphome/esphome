#ifdef USE_CAMERA_CONVERTER

#include "converter.h"

namespace esphome::camera_pipeline {

Converter::Converter(camera::PixelFormat format, camera::BufferImpl *output) {
  this->format_ = format;
  this->output_image_ = output;
}

void Converter::log_config() {
  ESP_LOGCONFIG(TAG,
                "Converter: %s\n"
                "  %s\n"
                "  Job: %s\n",
                this->get_id(), to_string(this->format_), YESNO(this->run_as_job_));
}

camera::ProcessorError Converter::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->output_spec_.format = this->format_;
  this->output_spec_.width = input_spec->width;
  this->output_spec_.height = input_spec->height;
  if (!this->output_image_->set_buffer_size(this->output_spec_.bytes_per_image()))
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  if (this->output_spec_.format == camera::PIXEL_FORMAT_GRAYSCALE) {
    uint8_t *output_data = this->output_image_->get_data();
    if (input_spec->format == camera::PIXEL_FORMAT_RGB565) {
      uint16_t *input_data = reinterpret_cast<uint16_t *>(input->get_data());
      for (uint16_t y = 0; y < input_spec->height; ++y) {
        for (uint16_t x = 0; x < input_spec->width; ++x) {
          *output_data = this->rgb565_to_grayscale(*input_data);
          *output_data = 0;
          ++output_data;
          ++input_data;
        }
      }
    }
  } else {
    this->cfg_.in_res = {static_cast<int16_t>(input_spec->width), static_cast<int16_t>(input_spec->height)};
    this->cfg_.in_pixel_fmt = to_internal_(input_spec->format);
    this->cfg_.out_pixel_fmt = to_internal_(output_spec_.format);

    if (!this->handle_)
      esp_imgfx_color_convert_open(&this->cfg_, &this->handle_);

    if (!this->handle_)
      return camera::PROCESSOR_ERROR_CONFIGURATION;

    if (input_spec->format == camera::PIXEL_FORMAT_GRAYSCALE)
      return camera::PROCESSOR_ERROR_CONFIGURATION;

    esp_imgfx_data_t in_image{input->get_data(), input->get_size()};
    esp_imgfx_data_t out_image{this->output_image_->get_data(), this->output_image_->get_size()};

    if (esp_imgfx_color_convert_process(this->handle_, &in_image, &out_image) != ESP_IMGFX_ERR_OK)
      return camera::PROCESSOR_ERROR_CONFIGURATION;
  }

  return camera::PROCESSOR_ERROR_SUCCESS;
}

esp_imgfx_pixel_fmt_t Converter::to_internal_(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_GRAYSCALE:
      return ESP_IMGFX_PIXEL_FMT_Y;
    case camera::PIXEL_FORMAT_RGB565:
      return ESP_IMGFX_PIXEL_FMT_BGR565_LE;
    case camera::PIXEL_FORMAT_BGR888:
      return ESP_IMGFX_PIXEL_FMT_RGB888;
  }

  return ESP_IMGFX_PIXEL_FMT_Y;
}

uint8_t Converter::rgb565_to_grayscale(uint16_t rgb565) {
  return (rgb565 >> 8) >> 2 + (rgb565 >> 3) >> 1 + (rgb565 << 3) >> 3;
}

}  // namespace esphome::camera_pipeline

#endif
