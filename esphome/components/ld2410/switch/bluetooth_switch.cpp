#include "bluetooth_switch.h"

namespace esphome::ld2410 {

void BluetoothSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_bluetooth(state);
}

}  // namespace esphome::ld2410
