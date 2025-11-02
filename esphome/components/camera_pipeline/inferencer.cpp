#ifdef USE_CAMERA_INFERENCER

#include "inferencer.h"
#include "esphome/core/log.h"

namespace esphome::camera_pipeline {

Inferencer::Inferencer(const uint8_t *rodata) { this->rodata_ = rodata; }

bool Inferencer::configure() {
  this->model_ = new dl::Model(reinterpret_cast<const char *>(this->rodata_), fbs::MODEL_LOCATION_IN_FLASH_RODATA, 0);
  if (!this->model_) {
    ESP_LOGE(TAG, "Cannot load model from .rodata.");
    return false;
  }

  std::map<std::string, dl::TensorBase *> inputs = this->model_->get_inputs();
  if (inputs.size() != 1) {
    ESP_LOGE(TAG, "Only models with one input tensor are supported.");
    return false;
  }

  this->input_tensor_ = this->model_->get_input();
  std::vector<int> shape = this->input_tensor_->get_shape();
  if (shape.size() == 4) {
    this->height_ = shape[1];
    this->width_ = shape[2];
    this->channels_ = shape[3];
  } else if (shape.size() == 3) {
    this->height_ = shape[0];
    this->width_ = shape[1];
    this->channels_ = shape[2];
  } else {
    ESP_LOGE(TAG, "Only Input tensors with 3 or 4 dimensions are supported.");
    return false;
  }

  std::map<std::string, dl::TensorBase *> model_outputs = this->model_->get_outputs();
  for (auto entry : model_outputs) {
    ESP_LOGI(TAG, "OUTPUT: %s SIZE: %i", entry.first.c_str(), entry.second->get_size());
  }

  return true;
}

void Inferencer::log_config() {
  ESP_LOGCONFIG(TAG,
                "Inferencer: %s\n"
                "  Mode: %s\n"
                "  Input Tensor:\n"
                "    Width: %i\n"
                "    Height: %i\n"
                "    Channels: %i\n"
                "    Type: %s\n"
                "  Job: %s\n",
                this->get_id(), to_string(this->mode_), this->width_, this->height_, this->channels_,
                this->input_tensor_->get_dtype_string(), YESNO(this->run_as_job_));
}

camera::ProcessorError Inferencer::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->model_->run(to_internal_(this->mode_));
  this->output_spec_ = *input_spec;
  this->output_image_ = input;
  return camera::PROCESSOR_ERROR_SUCCESS;
}

dl::image::pix_type_t Inferencer::convert(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_GRAYSCALE:
      return dl::image::DL_IMAGE_PIX_TYPE_GRAY;
    case camera::PIXEL_FORMAT_RGB565:
      return dl::image::DL_IMAGE_PIX_TYPE_RGB565;
    case camera::PIXEL_FORMAT_BGR888:
      return dl::image::DL_IMAGE_PIX_TYPE_RGB888;
  }

  return dl::image::DL_IMAGE_PIX_TYPE_GRAY;
}

dl::runtime_mode_t Inferencer::to_internal_(InferencerMode mode) {
  switch (mode) {
    case AUTO:
      return dl::runtime_mode_t::RUNTIME_MODE_AUTO;
    case SINGLE_CORE:
      return dl::runtime_mode_t::RUNTIME_MODE_SINGLE_CORE;
    case MULTI_CORE:
      return dl::runtime_mode_t::RUNTIME_MODE_MULTI_CORE;
  }

  return dl::runtime_mode_t::RUNTIME_MODE_AUTO;
}

}  // namespace esphome::camera_pipeline

#endif
