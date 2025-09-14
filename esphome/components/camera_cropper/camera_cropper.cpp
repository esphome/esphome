#include "camera_cropper.h"
#include "esphome/core/log.h"

namespace esphome::camera_cropper {

static const char *const TAG = "camera.cropper";

CameraCropper::CameraCropper(camera::CameraImageSpec *spec, camera::Buffer *output, int crop_x, int crop_y,
                             int crop_width, int crop_height) {
  this->output_spec_ = spec;
  this->output_image_ = output;
  this->crop_x_ = crop_x;
  this->crop_y_ = crop_y;
  this->crop_width_ = crop_width;
  this->crop_height_ = crop_height;
}

size_t CameraCropper::process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) {
  // Validate crop region
  if (crop_x_ + crop_width_ > input_spec->width || crop_y_ + crop_height_ > input_spec->height) {
    ESP_LOGE(TAG, "Crop region exceeds source image dimensions!");
    return 0;
  }

  // Set output spec based on input format (but keep the allocated dimensions)
  this->output_spec_->format = input_spec->format;

  // Set bytes per pixel based on image type
  size_t bytes_per_pixel = input_spec->bytes_per_pixel();

  // Get source image data
  const uint8_t *source_data = input->get_data();
  size_t source_width = input_spec->width;

  // Get destination buffer
  uint8_t *dest_data = this->output_image_->get_data();

  // Perform cropping
  for (size_t y = 0; y < crop_height_; y++) {
    size_t source_y = crop_y_ + y;
    size_t dest_y = this->flip_y_ ? (crop_height_ - 1 - y) : y;

    for (size_t x = 0; x < crop_width_; x++) {
      size_t source_x = crop_x_ + x;
      size_t dest_x = this->flip_x_ ? (crop_width_ - 1 - x) : x;

      size_t source_idx = (source_y * source_width + source_x) * bytes_per_pixel;
      size_t dest_idx = (dest_y * crop_width_ + dest_x) * bytes_per_pixel;

      // Copy pixel data
      for (size_t b = 0; b < bytes_per_pixel; b++) {
        dest_data[dest_idx + b] = source_data[source_idx + b];
      }
    }
  }

  return this->output_spec_->bytes_per_image();
}

}  // namespace esphome::camera_cropper
