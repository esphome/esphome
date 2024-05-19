#include "tlc5971_output.h"

namespace esphome {
namespace tlc5971 {

void TLC5971Channel::write_state(float state) {
  auto amount = static_cast<uint16_t>(state * 0xffff);
  this->parent_->set_channel_value(this->channel_, amount);
}

}  // namespace tlc5971
}  // namespace esphome
