#ifdef USE_CAMERA_ROTATOR

#include "rotator.h"

namespace esphome::camera_pipeline {

Rotator::Rotator(camera::BufferImpl *output, uint16_t rotation) {
  this->output_image_ = output;
  this->rotation_ = rotation;
}

void Rotator::log_config() {
  ESP_LOGCONFIG(TAG,
                "Rotator: %s\n"
                "  Angle: %u° CW\n"
                "  Job: %s\n",
                this->get_id(), this->rotation_, YESNO(this->run_as_job_));
}

camera::ProcessorError Rotator::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->cfg_.in_res = {static_cast<int16_t>(input_spec->width), static_cast<int16_t>(input_spec->height)};
  this->cfg_.degree = this->rotation_;
  this->cfg_.in_pixel_fmt = to_internal_(input_spec->format);
  if (!this->handle_)
    esp_imgfx_rotate_open(&this->cfg_, &this->handle_);

  if (!this->handle_)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  esp_imgfx_resolution_t resolution{};
  esp_imgfx_rotate_get_rotated_resolution(this->handle_, &resolution);
  this->output_spec_.width = resolution.width;
  this->output_spec_.height = resolution.height;
  this->output_spec_.format = input_spec->format;
  if (!this->output_image_->set_buffer_size(this->output_spec_.bytes_per_image()))
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  if (this->clear_)
    memset(this->output_image_->get_data(), 0, this->output_image_->get_size());

  esp_imgfx_data_t in_image{input->get_data(), input->get_size()};
  esp_imgfx_data_t out_image{this->output_image_->get_data(), this->output_image_->get_size()};

  if (esp_imgfx_rotate_process(this->handle_, &in_image, &out_image) != ESP_IMGFX_ERR_OK)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  return camera::PROCESSOR_ERROR_SUCCESS;
}

esp_imgfx_pixel_fmt_t Rotator::to_internal_(camera::PixelFormat format) {
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

}  // namespace esphome::camera_pipeline

#endif
