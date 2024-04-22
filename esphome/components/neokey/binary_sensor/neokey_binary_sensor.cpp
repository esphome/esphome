#include "neokey_binary_sensor.h"

namespace esphome {
namespace neokey {

void NeoKeyBinarySensor::keys_update(uint8_t keys) {
  bool pressed = keys & (1 << key_);
  if (pressed != this->state)
    this->publish_state(pressed);
}

}  // namespace neokey
}  // namespace esphome
