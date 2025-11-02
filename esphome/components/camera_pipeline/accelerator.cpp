#ifdef USE_ESP32_VARIANT_ESP32P4
#include "accelerator.h"
#include <utility>

namespace esphome::camera_pipeline {

Accelerator::Accelerator(camera::BufferImpl *output) { this->output_image_ = output; }

bool Accelerator::configure() {
  ppa_client_config_t config;
  config.oper_type = this->to_internal_(this->mode_);
  config.max_pending_trans_num = 1;
  config.data_burst_length = PPA_DATA_BURST_LENGTH_128;
  return ppa_register_client(&config, &this->ppa_client_) == ESP_OK;
}

void Accelerator::log_config() {
  ESP_LOGCONFIG(TAG,
                "Accelerator: %s\n"
                "  Scale X: %.4f, Y: %.4f\n"
                "  Flip X: %s, Y: %s\n"
                "  Angle: %s° CCW\n"
                "  Format: %s\n"
                "  Job: %s\n",
                this->get_id(), this->scale_x_, this->scale_y_, YESNO(this->flip_x_), YESNO(this->flip_y_),
                to_string(this->rotation_), this->format_set_ ? to_string(this->format_) : "AUTO",
                YESNO(this->run_as_job_));
}

camera::ProcessorError Accelerator::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  this->output_spec_.width = this->input_spec_->width * this->scale_x_;
  this->output_spec_.height = this->input_spec_->height * this->scale_y_;
  if (this->rotation_ == ACCELERATOR_ANGLE_90 || this->rotation_ == ACCELERATOR_ANGLE_270)
    std::swap(this->output_spec_.width, this->output_spec_.height);

  this->output_spec_.format = input_spec->format;
  if (this->format_set_)
    this->output_spec_.format = this->format_;

  if (!this->output_image_->set_buffer_size(this->output_spec_.bytes_per_image()))
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  // Avoid that ppa_do_scale_rotate_mirror crashes.
  if (this->output_spec_.format == camera::PIXEL_FORMAT_GRAYSCALE)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  ppa_srm_oper_config_t oper = {
      .in = {.buffer = input->get_data(),
             .pic_w = input_spec->width,
             .pic_h = input_spec->height,
             .block_w = input_spec->width,
             .block_h = input_spec->height,
             .block_offset_x = 0,
             .block_offset_y = 0,
             .srm_cm = static_cast<ppa_srm_color_mode_t>(this->to_internal_(input_spec->format))},
      .out = {.buffer = this->output_image_->get_data(),
              .buffer_size = this->output_image_->get_max_size(),
              .pic_w = this->output_spec_.width,
              .pic_h = this->output_spec_.height,
              .block_offset_x = 0,
              .block_offset_y = 0,
              .srm_cm = static_cast<ppa_srm_color_mode_t>(this->to_internal_(this->output_spec_.format))},
      .rotation_angle = this->to_internal_(this->rotation_),
      .scale_x = this->scale_x_,
      .scale_y = this->scale_y_,
      .mirror_x = this->flip_x_,
      .mirror_y = this->flip_y_,
      .rgb_swap = false,
      .byte_swap = false,
      .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
      .mode = PPA_TRANS_MODE_BLOCKING,
      .user_data = nullptr};

  if (ppa_do_scale_rotate_mirror(this->ppa_client_, &oper) != ESP_OK)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  return camera::PROCESSOR_ERROR_SUCCESS;
}

ppa_operation_t Accelerator::to_internal_(AcceleratorMode mode) {
  switch (mode) {
    case ACCELERATOR_SCALE_ROTATE_MIRROR:
      return PPA_OPERATION_SRM;
    case ACCELERATOR_BLEND:
      return PPA_OPERATION_BLEND;
    case ACCELERATOR_FILL:
      return PPA_OPERATION_FILL;
  }

  return PPA_OPERATION_INVALID;
}

ppa_srm_rotation_angle_t Accelerator::to_internal_(AcceleratorRotation rotation) {
  switch (rotation) {
    case ACCELERATOR_ANGLE_0:
      return PPA_SRM_ROTATION_ANGLE_0;
    case ACCELERATOR_ANGLE_90:
      return PPA_SRM_ROTATION_ANGLE_90;
    case ACCELERATOR_ANGLE_180:
      return PPA_SRM_ROTATION_ANGLE_180;
    case ACCELERATOR_ANGLE_270:
      return PPA_SRM_ROTATION_ANGLE_270;
  }

  return PPA_SRM_ROTATION_ANGLE_0;
}

uint32_t Accelerator::to_internal_(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_GRAYSCALE:
      return COLOR_TYPE_ID(COLOR_SPACE_GRAY, COLOR_PIXEL_GRAY8);
    case camera::PIXEL_FORMAT_RGB565:
      return COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565);
    case camera::PIXEL_FORMAT_BGR888:
      return COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888);
  }

  return COLOR_TYPE_ID(COLOR_SPACE_GRAY, COLOR_PIXEL_GRAY8);
}

}  // namespace esphome::camera_pipeline

#endif
