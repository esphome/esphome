#pragma once

#include "esphome/components/camera/processor.h"

namespace esphome::camera_processor {

class ProcessorBase : public camera::Processor {
 public:
  // ------ Processor ------
  void process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) override {
    this->process_pixels_base(input_spec, input);
    this->process_callback_(get_output_image_spec(), get_output_image());
  }
  // ------------------------
  virtual void process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) = 0;
  void add_process_callback(std::function<void(camera::CameraImageSpec *spec, camera::Buffer *)> &&callback) {
    this->process_callback_.add(std::move(callback));
  }

 protected:
  CallbackManager<void(camera::CameraImageSpec *spec, camera::Buffer *)> process_callback_{};
};

}  // namespace esphome::camera_processor
