#ifdef USE_CAMERA_CROPPER

#include "cropper.h"
#include "esphome/core/log.h"

namespace esphome::camera_pipeline {

Cropper::Cropper(uint16_t width, uint16_t height, camera::BufferImpl *output, int crop_x, int crop_y) {
  this->output_spec_.width = width;
  this->output_spec_.height = height;
  this->output_image_ = output;
  this->crop_x_ = crop_x;
  this->crop_y_ = crop_y;
}

void Cropper::log_config() {
  ESP_LOGCONFIG(TAG,
                "Cropper: %s\n"
                "  Width: %u\n"
                "  Height: %u\n"
                "  Crop X: %u, Y: %u\n"
                "  Job: %s\n",
                this->get_id(), this->output_spec_.width, this->output_spec_.height, this->crop_x_, this->crop_y_,
                YESNO(this->run_as_job_));
}

camera::ProcessorError Cropper::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->output_spec_.format = input_spec->format;
  if (!this->output_image_->set_buffer_size(this->output_spec_.bytes_per_image()))
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  this->cfg_.in_res = {static_cast<int16_t>(input_spec->width), static_cast<int16_t>(input_spec->height)};
  this->cfg_.cropped_res = {static_cast<int16_t>(output_spec_.width), static_cast<int16_t>(output_spec_.height)};
  this->cfg_.x_pos = this->crop_x_;
  this->cfg_.y_pos = this->crop_y_;
  this->cfg_.in_pixel_fmt = to_internal_(input_spec->format);
  if (!this->handle_)
    esp_imgfx_crop_open(&this->cfg_, &this->handle_);

  if (!this->handle_)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  esp_imgfx_data_t in_image{input->get_data(), input->get_size()};
  esp_imgfx_data_t out_image{this->output_image_->get_data(), this->output_image_->get_size()};

  if (esp_imgfx_crop_process(this->handle_, &in_image, &out_image) != ESP_IMGFX_ERR_OK)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  return camera::PROCESSOR_ERROR_SUCCESS;
}

esp_imgfx_pixel_fmt_t Cropper::to_internal_(camera::PixelFormat format) {
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
