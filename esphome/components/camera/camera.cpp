#include "camera.h"

namespace esphome {
namespace camera {

Camera *Camera::global_camera = nullptr;

Camera::Camera() {
  global_camera = this;
}

}  // namespace camera
}  // namespace esphome
