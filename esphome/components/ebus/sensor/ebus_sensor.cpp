
#include "ebus_sensor.h"

namespace esphome {
namespace ebus {

void EbusSensor::process_received(Telegram telegram) {
  if (!is_mine(telegram)) {
    return;
  }
  this->publish_state(to_float(telegram, this->response_position_, this->response_bytes_, this->response_divider_));
}

float EbusSensor::to_float(Telegram &telegram, uint8_t start, uint8_t length, float divider) {
  return get_response_bytes(telegram, start, length) / divider;
}

}  // namespace ebus
}  // namespace esphome
