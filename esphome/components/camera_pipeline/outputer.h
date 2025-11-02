#pragma once

#ifdef USE_CAMERA_OUTPUTER

#include "esphome/components/camera/encoder.h"
#include "esphome/components/camera/buffer_impl.h"
#include "esphome/components/camera/buffer_pool.h"
#include "esphome/components/camera/output.h"
#include "esphome/components/camera/requester_flags.h"
#include "processor_automation.h"

namespace esphome::camera_pipeline {

/// Output node at the end of the camera pipeline.
/// Provides final image data to CameraRequesters, optionally using an encoder.
class Outputer : public ProcessorAutomation<Outputer>, public camera::Output {
 public:
  /// Set optional encoder used to compress pixel data before output.
  void set_encoder(camera::Encoder *encoder) { this->encoder_ = encoder; }
  /// Add a specific CameraRequester as output target.
  void set_requester(camera::CameraRequester requester) { this->flags_.add(requester); }
  /// Enable output for all CameraRequesters.
  void set_requester_all() { this->flags_.add_all(); }
  /// Set the number of output buffers.
  void set_buffers(uint8_t buffers) { this->buffers_ = buffers; }
  /// Set the size of each output buffer in bytes.
  void set_buffer_size(size_t buffer_size) { this->buffer_size_ = buffer_size; }
  // ------ Processor ------
  bool configure() override;
  camera::CameraImageSpec *get_output_image_spec() override { return &this->output_spec_; }
  camera::Buffer *get_output_image() override { return this->output_buffer_; }
  camera::ImageFormat get_output_image_format() override { return camera::ImageFormat::IMAGE_FORMAT_JPEG; }
  void log_config() override;
  // ---- ProcessorBase ----
  camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  camera::ProcessorError process_compressed_image_base(camera::ImageFormat input_format,
                                                       camera::Buffer *input) override;
  // ------- Output --------
  std::shared_ptr<camera::CameraImageImpl> get_image() override;
  // -----------------------

 protected:
  uint8_t buffers_{};
  size_t buffer_size_{};
  camera::RequesterFlags flags_{};
  camera::Encoder *encoder_{};
  camera::CameraImageSpec output_spec_{};
  camera::BufferImpl *output_buffer_{};
  camera::BufferPool<camera::BufferImpl> output_buffers_{};
};

}  // namespace esphome::camera_pipeline

#endif
