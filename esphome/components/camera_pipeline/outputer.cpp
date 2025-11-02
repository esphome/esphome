#ifdef USE_CAMERA_OUTPUTER

#include "outputer.h"
#include "esphome/core/log.h"
#include "esphome/components/camera/camera_image_impl.h"

namespace esphome::camera_pipeline {

bool Outputer::configure() {
  this->output_buffers_.init(this->buffers_,
                             [this]() -> camera::BufferImpl * { return new camera::BufferImpl(this->buffer_size_); });
  return true;
}

void Outputer::log_config() {
  ESP_LOGCONFIG(TAG,
                "Outputer: %s\n"
                "  Buffers: %u\n"
                "  Size: %zu\n"
                "  Requester: %s\n"
                "  Encoder: %s\n"
                "  Job: %s\n",
                this->get_id(), this->buffers_, this->buffer_size_, this->flags_.to_string(),
                YESNO(this->encoder_ != nullptr), YESNO(this->run_as_job_));

  if (this->encoder_)
    this->encoder_->dump_config();
}

camera::ProcessorError Outputer::process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  if (!this->encoder_)
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  if (!this->output_buffer_)
    this->output_buffer_ = this->output_buffers_.acquire();

  if (!this->output_buffer_)
    return camera::PROCESSOR_ERROR_RETRY_FRAME;

  this->encoder_->set_output_buffer(this->output_buffer_);
  camera::EncoderError error = this->encoder_->encode_pixels(input_spec, input);
  switch (error) {
    case camera::ENCODER_ERROR_SUCCESS: {
    } break;
    case camera::ENCODER_ERROR_SKIP_FRAME: {
      return camera::PROCESSOR_ERROR_SKIP_FRAME;
    } break;
    case camera::ENCODER_ERROR_RETRY_FRAME: {
      return camera::PROCESSOR_ERROR_RETRY_FRAME;
    } break;
    case camera::ENCODER_ERROR_CONFIGURATION: {
      return camera::PROCESSOR_ERROR_CONFIGURATION;
    } break;
  }

  this->output_spec_ = *input_spec;
  return camera::PROCESSOR_ERROR_SUCCESS;
}

camera::ProcessorError Outputer::process_compressed_image_base(camera::ImageFormat input_format,
                                                               camera::Buffer *input) {
  if (!this->output_buffer_)
    this->output_buffer_ = this->output_buffers_.acquire();

  if (!this->output_buffer_)
    return camera::PROCESSOR_ERROR_RETRY_FRAME;

  if (!this->output_buffer_->set_buffer_size(input->get_size()))
    return camera::PROCESSOR_ERROR_CONFIGURATION;

  std::memcpy(this->output_buffer_->get_data(), input->get_data(), input->get_size());
  return camera::PROCESSOR_ERROR_SUCCESS;
}

std::shared_ptr<camera::CameraImageImpl> Outputer::get_image() {
  std::shared_ptr<camera::CameraImageImpl> image = std::make_shared<camera::CameraImageImpl>();
  image->set_buffer(this->output_buffer_);
  image->on_delete([this, output_buffer = this->output_buffer_](camera::CameraImageImpl *camera_image) {
    this->output_buffers_.release(output_buffer);
  });
  this->output_buffer_ = nullptr;
  return image;
}

}  // namespace esphome::camera_pipeline

#endif
