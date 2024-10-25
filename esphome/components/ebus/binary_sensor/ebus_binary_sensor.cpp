
#include "ebus_binary_sensor.h"

namespace esphome {
namespace ebus {

void EbusBinarySensor::process_received(Telegram telegram) {
  if (!is_mine(telegram)) {
    return;
  }
  uint8_t result = (uint8_t) this->get_response_bytes(telegram, this->response_position_, 1);
  this->publish_state(result & this->mask_);
}

}  // namespace ebus
}  // namespace esphome
