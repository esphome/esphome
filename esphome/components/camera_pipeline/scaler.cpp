#ifdef USE_CAMERA_SCALER

#include "scaler.h"

namespace esphome::camera_pipeline {

Scaler::Scaler(ScalerAlgorithm algorithm, uint16_t width, uint16_t height, camera::BufferImpl *output) {
  this->algorithm_ = algorithm;
  this->output_spec_.width = width;
  this->output_spec_.height = height;
  this->output_image_ = output;
}

void Scaler::log_config() {
  ESP_LOGCONFIG(TAG,
                "Scaler: %s\n"
                "  %s\n"
                "  Width: %u\n"
                "  Height: %u\n"
                "  Job: %s\n",
                this->get_id(), to_string(this->algorithm_), this->output_spec_.width, this->output_spec_.height,
                YESNO(this->run_as_job_));
}

camera::ProcessorError Scaler::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->output_spec_.format = input_spec->format;
  if (!this->output_image_->set_buffer_size(this->output_spec_.bytes_per_image()))
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  this->cfg_.in_res = {static_cast<int16_t>(input_spec->width), static_cast<int16_t>(input_spec->height)};
  this->cfg_.scale_res = {static_cast<int16_t>(output_spec_.width), static_cast<int16_t>(output_spec_.height)};
  this->cfg_.in_pixel_fmt = to_internal_(input_spec->format);
  this->cfg_.filter_type = this->to_internal_(algorithm_);
  if (!this->handle_) {
    esp_imgfx_scale_open(&this->cfg_, &this->handle_);
    if (this->algorithm_ == DOWN_RESAMPLE &&
        (input_spec->width < output_spec_.width || input_spec->height < output_spec_.height))
      ESP_LOGE(TAG, "%s: requires input (%ux%u) > output (%ux%u). Use %s.", to_string(this->algorithm_),
               input_spec->width, input_spec->height, output_spec_.width, output_spec_.height, to_string(BILINEAR));
  }

  if (!this->handle_)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  esp_imgfx_data_t in_image{input->get_data(), input->get_size()};
  esp_imgfx_data_t out_image{this->output_image_->get_data(), this->output_image_->get_size()};

  if (esp_imgfx_scale_process(this->handle_, &in_image, &out_image) != ESP_IMGFX_ERR_OK)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  return camera::PROCESSOR_ERROR_SUCCESS;
}

esp_imgfx_scale_filter_type_t Scaler::to_internal_(ScalerAlgorithm algorithm) {
  switch (algorithm) {
    case DOWN_RESAMPLE:
      return ESP_IMGFX_SCALE_FILTER_TYPE_DOWN_RESAMPLE;
    case BILINEAR:
      return ESP_IMGFX_SCALE_FILTER_TYPE_BILINEAR;
  }

  return ESP_IMGFX_SCALE_FILTER_TYPE_DOWN_RESAMPLE;
}

esp_imgfx_pixel_fmt_t Scaler::to_internal_(camera::PixelFormat format) {
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
