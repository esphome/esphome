#ifdef USE_CAMERA_OVERLAYER

#include "overlayer.h"
#include "esphome/core/log.h"

namespace esphome::camera_pipeline {

bool Overlayer::configure() {
  this->set_auto_clear(false);
  this->disable_loop();
  return true;
}

void Overlayer::log_config() {
  ESP_LOGCONFIG(TAG,
                "Overlayer: %s\n"
                "  Job: %s\n",
                this->get_id(), YESNO(this->run_as_job_));
}

camera::ProcessorError Overlayer::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->output_spec_ = *input_spec;
  this->output_image_ = input;
  this->display_type_ = this->to_internal_(this->output_spec_.format);
  if (this->copy_buffer_) {
    if (!this->copy_buffer_->set_buffer_size(input_spec->bytes_per_image()))
      return camera::PROCESSOR_ERROR_CONFIGURATION;

    std::memcpy(this->copy_buffer_->get_data(), input->get_data(), input->get_size());
    this->output_image_ = this->copy_buffer_;
  }

  return camera::PROCESSOR_ERROR_SUCCESS;
}

void Overlayer::draw_pixel_at(int x, int y, Color color) {
  if (x >= this->output_spec_.width || y >= this->output_spec_.height)
    return;

  uint8_t *data = this->output_image_->get_data();
  switch (this->output_spec_.format) {
    case camera::PIXEL_FORMAT_GRAYSCALE: {
      data[y * this->output_spec_.bytes_per_row() + x] = color.w;
    } break;
    case camera::PIXEL_FORMAT_RGB565: {
      int idx = y * this->output_spec_.width + x;
      reinterpret_cast<uint16_t *>(data)[idx] = (color.r & 0xF8) << 8 | (color.g & 0xFC) << 3 | (color.b & 0xF8) >> 3;
    } break;
    case camera::PIXEL_FORMAT_BGR888: {
      int idx = (y * this->output_spec_.width + x) * 3;
      data[idx] = color.b;
      data[idx + 1] = color.g;
      data[idx + 2] = color.r;
    } break;
  }
}

display::DisplayType Overlayer::to_internal_(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_GRAYSCALE:
      return display::DISPLAY_TYPE_GRAYSCALE;
    case camera::PIXEL_FORMAT_RGB565:
      return display::DISPLAY_TYPE_COLOR;
    case camera::PIXEL_FORMAT_BGR888:
      return display::DISPLAY_TYPE_COLOR;
  }

  return display::DISPLAY_TYPE_GRAYSCALE;
}

}  // namespace esphome::camera_pipeline

#endif
