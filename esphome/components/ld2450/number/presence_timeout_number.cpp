#include "presence_timeout_number.h"

namespace esphome {
namespace ld2450 {

void PresenceTimeoutNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_presence_timeout();
}

}  // namespace ld2450
}  // namespace esphome
