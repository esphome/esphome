#include "tm1638_key.h"

namespace esphome {
namespace tm1638 {

void TM1638Key::keys_update(uint8_t keys) {
  bool pressed = keys & (1 << key_code_);
  if (pressed != this->state)
    this->publish_state(pressed);
}

}  // namespace tm1638
}  // namespace esphome
