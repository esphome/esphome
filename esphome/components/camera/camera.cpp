#include "camera.h"

namespace esphome {
namespace camera {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Camera *Camera::global_camera = nullptr;

Camera::Camera() { global_camera = this; }

}  // namespace camera
}  // namespace esphome
