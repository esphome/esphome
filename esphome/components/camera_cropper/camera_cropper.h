// camera_cropper.h
#pragma once

#include "esphome/components/camera/processor.h"

namespace esphome {
namespace camera_cropper {

class CameraCropper : public camera::Processor {
 public:
  CameraCropper(camera::CameraImageSpec *spec, camera::CameraImage *output, int crop_x, int crop_y, int crop_width, int crop_height);
  
  // Processor interface methods
  size_t process_pixels(camera::CameraImageSpec *input_spec, camera::CameraImage *input) override;
  camera::CameraImageSpec *get_output_image_spec() override { return this->output_spec_; }
  camera::CameraImage *get_output_image() override { return this->output_image_; }

  void set_flip_x(bool flip) { this->flip_x_ = flip; }
  void set_flip_y(bool flip) { this->flip_y_ = flip; }

 protected:
  int crop_x_;
  int crop_y_;
  int crop_width_;
  int crop_height_;
  bool flip_x_{};
  bool flip_y_{};
  camera::CameraImageSpec *output_spec_{};
  camera::CameraImage *output_image_{};
};

}  // namespace camera_cropper
}  // namespace esphome