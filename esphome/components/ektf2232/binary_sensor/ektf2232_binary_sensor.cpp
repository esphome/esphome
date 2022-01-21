#include "ektf2232_binary_sensor.h"

namespace esphome {
namespace ektf2232 {

void EKTF2232Button::touch(TouchPoint tp) {
  bool touched = (tp.x >= this->x_min_ && tp.x <= this->x_max_ && tp.y >= this->y_min_ && tp.y <= this->y_max_);

  if (touched) {
    this->publish_state(true);
  } else {
    release();
  }
}

void EKTF2232Button::release() { this->publish_state(false); }

}  // namespace ektf2232
}  // namespace esphome
