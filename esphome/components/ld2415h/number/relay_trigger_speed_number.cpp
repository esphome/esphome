#include "relay_trigger_speed_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.relay_trigger_speed_number";

void RelayTriggerSpeedNumber::control(float speed) {
  this->publish_state(speed);
  this->parent_->set_relay_trigger_speed(speed);
}

}  // namespace ld2415h
}  // namespace esphome
