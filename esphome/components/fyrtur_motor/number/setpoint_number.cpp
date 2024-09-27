#include "setpoint_number.h"

namespace esphome {
namespace fyrtur_motor {

void SetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->update_setpoints();
}

}  // namespace fyrtur_motor
}  // namespace esphome
