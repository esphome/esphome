#include "tlc5947_output.h"

namespace esphome::tlc5947 {

void TLC5947Channel::write_state(float state) {
  auto amount = static_cast<uint16_t>(state * 0xfff);
  this->parent_->set_channel_value(this->channel_, amount);
}

}  // namespace esphome::tlc5947
