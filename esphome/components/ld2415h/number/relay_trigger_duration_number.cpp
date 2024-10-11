#include "relay_trigger_duration_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.relay_trigger_duration_number";

void RelayTriggerDurationNumber::control(float duration) {
  this->publish_state(duration);
  this->parent_->set_relay_trigger_duration(duration);
}

}  // namespace ld2415h
}  // namespace esphome
