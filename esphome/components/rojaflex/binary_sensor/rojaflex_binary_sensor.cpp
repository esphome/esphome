#include "rojaflex_binary_sensor.h"

namespace esphome::rojaflex {

void RojaflexBinarySensor::update() {
  if (this->parent_ == nullptr) {
    return;
  }
  switch (this->type_) {
    case RojaflexBinarySensorType::LAST_TX_OK:
      this->publish_state(this->parent_->get_last_tx_ok());
      break;
  }
}

}  // namespace esphome::rojaflex
