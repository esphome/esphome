#include "camera.h"

namespace esphome {
namespace camera {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Camera *Camera::global_camera = nullptr;

Camera::Camera() {
  if (global_camera != nullptr) {
    this->status_set_error(LOG_STR("Multiple cameras are configured, but only one is supported."));
    this->mark_failed();
    return;
  }

  global_camera = this;
}

Camera *Camera::instance() { return global_camera; }

}  // namespace camera
}  // namespace esphome
