#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA_IMAGE

#include "esphome/components/camera/camera.h"
#include "esphome/components/image/image.h"
#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "transform.h"

#include <memory>
#include <utility>

namespace esphome::camera_image {

/// An ``image`` platform that requests camera frames and decodes them into its buffer.
class CameraImage final : public PollingComponent, public runtime_image::RuntimeImage, public camera::CameraListener {
 public:
  CameraImage(camera::Camera *camera, int width, int height, runtime_image::ImageFormat format, image::ImageType type,
              image::Transparency transparency, image::Image *placeholder, bool is_big_endian, uint32_t request_timeout,
              const Transform &transform);

  void setup() override;
  void update() override;
  void dump_config() override;

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;

  template<typename F> void add_on_image_callback(F &&callback) {
    this->on_image_callback_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_error_callback(F &&callback) {
    this->on_error_callback_.add(std::forward<F>(callback));
  }

 protected:
  bool decode_frame_(const uint8_t *data, size_t length);
#ifdef USE_CAMERA_IMAGE_TRANSFORM
  bool apply_transform_();
#endif

  camera::Camera *camera_;
#ifdef USE_CAMERA_IMAGE_TRANSFORM
  Transform transform_;
  /// Size of the decoded frame, kept so a transformed frame does not resize the buffer.
  int decode_width_{0};
  int decode_height_{0};
#endif
  uint32_t request_timeout_;
  bool waiting_{false};
  LazyCallbackManager<void()> on_image_callback_{};
  LazyCallbackManager<void()> on_error_callback_{};
};

}  // namespace esphome::camera_image

#endif  // USE_CAMERA_IMAGE
