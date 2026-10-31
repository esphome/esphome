#include "locator.h"

namespace esphome::duco {

void DucoLocator::write_state(bool state) {
  this->parent_->send(nullptr, 0x10, {0x04, this->address_, static_cast<uint8_t>(state)});
  this->publish_state(state);
}

}  // namespace esphome::duco
