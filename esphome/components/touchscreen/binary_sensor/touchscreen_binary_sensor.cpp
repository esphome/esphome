#include "touchscreen_binary_sensor.h"

namespace esphome {
namespace touchscreen {

void TouchscreenBinarySensor::touch(TouchPoint tp) {
  bool touched = (tp.x >= this->x_min_ && tp.x <= this->x_max_ && tp.y >= this->y_min_ && tp.y <= this->y_max_);
#ifdef USE_DISPLAY
  if ((this->page_ != nullptr) && (this->parent_->get_display() != nullptr)) {
    touched &= this->page_ == this->parent_->get_display()->get_active_page();
  }
#endif
  if (touched) {
    this->publish_state(true);
  } else {
    release();
  }
}

void TouchscreenBinarySensor::release() { this->publish_state(false); }

}  // namespace touchscreen
}  // namespace esphome
