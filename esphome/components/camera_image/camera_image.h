#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA_IMAGE

#include "esphome/components/camera/camera.h"
#include "esphome/components/image/image.h"
#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <utility>

namespace esphome::camera_image {

/// An ``image`` platform that requests camera frames and decodes them into its buffer.
class CameraImage : public PollingComponent, public runtime_image::RuntimeImage, public camera::CameraListener {
 public:
  CameraImage(camera::Camera *camera, int width, int height, runtime_image::ImageFormat format, image::ImageType type,
              image::Transparency transparency, image::Image *placeholder, bool is_big_endian);
  ~CameraImage();

  void setup() override;
  void update() override;
  void dump_config() override;

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;

  void set_crop(int x, int y, int width, int height) {
    this->crop_x_ = x;
    this->crop_y_ = y;
    this->crop_width_ = width;
    this->crop_height_ = height;
    this->has_crop_ = true;
  }

  void set_rotation(int rotation) { this->rotation_ = rotation; }

  /// Size of the crop/rotation output, allocated in setup().
  void set_transform_size(int width, int height) {
    this->transform_width_ = width;
    this->transform_height_ = height;
  }

  template<typename F> void add_on_image_callback(F &&callback) {
    this->on_image_callback_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_error_callback(F &&callback) {
    this->on_error_callback_.add(std::forward<F>(callback));
  }

 protected:
  bool decode_frame_(const uint8_t *data, size_t length);
  bool apply_crop_rotation_();

  camera::Camera *camera_;
  bool waiting_{false};
  bool has_crop_{false};
  int crop_x_{0};
  int crop_y_{0};
  int crop_width_{0};
  int crop_height_{0};
  int rotation_{0};
  int transform_width_{0};
  int transform_height_{0};
  /// Size of the decoded frame, kept so a transformed frame does not resize the buffer.
  int decode_width_{0};
  int decode_height_{0};
  /// Scratch buffer for rotation, allocated in setup() through RAMAllocator.
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};
  LazyCallbackManager<void()> on_image_callback_{};
  LazyCallbackManager<void()> on_error_callback_{};
};

}  // namespace esphome::camera_image

#endif  // USE_CAMERA_IMAGE
