#include "camera.h"

namespace esphome {
namespace camera {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Camera *Camera::global_camera = nullptr;

Camera::Camera() {
  if (global_camera != nullptr) {
    this->status_set_error("Multiple cameras are configured, but only one is supported.");
    this->mark_failed();
    return;
  }

  global_camera = this;
}

void Camera::add_capture_callback(
    std::function<void(Buffer &, CameraImageSpec, CameraIncrementalContext &)> &&callback) {
  this->image_capture_callback_.add(std::move(callback));
}

void Camera::add_overlay_callback(
    std::function<void(Buffer &, CameraImageSpec, CameraIncrementalContext &)> &&callback) {
  this->overlay_callback_.add(std::move(callback));
}

void Camera::add_image_callback(std::function<void(std::shared_ptr<CameraImage>)> &&callback) {
  this->new_image_callback_.add(std::move(callback));
}

void Camera::add_stream_start_callback(std::function<void()> &&callback) {
  this->stream_start_callback_.add(std::move(callback));
}

void Camera::add_stream_stop_callback(std::function<void()> &&callback) {
  this->stream_stop_callback_.add(std::move(callback));
}

Camera *Camera::instance() { return global_camera; }

}  // namespace camera
}  // namespace esphome
