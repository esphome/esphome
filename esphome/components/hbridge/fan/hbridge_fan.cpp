#include "hbridge_fan.h"
#include "esphome/components/fan/fan_helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "fan.hbridge";

fan::FanCall HBridgeFan::brake() {
  hbridge_set_state(HBRIDGE_MODE_SHORT, 0); //Break motor by shorting windings
  ESP_LOGD(TAG, "Braking");
  return this->make_call().set_state(false);
}

void HBridgeFan::setup() {
  HBridge::setup();
  
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
    this->write_state_();
  }
}

void HBridgeFan::dump_config() {
  LOG_FAN("", "H-Bridge Fan", this);
  HBridge::dump_config();
}
fan::FanTraits HBridgeFan::get_traits() {
  return fan::FanTraits(this->oscillating_ != nullptr, true, true, this->speed_count_);
}
void HBridgeFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (call.get_speed().has_value())
    this->speed = *call.get_speed();
  if (call.get_oscillating().has_value())
    this->oscillating = *call.get_oscillating();
  if (call.get_direction().has_value())
    this->direction = *call.get_direction();

  this->write_state_();
  this->publish_state();
}
void HBridgeFan::write_state_() {
  float speed = this->state ? static_cast<float>(this->speed) / static_cast<float>(this->speed_count_) : 0.0f;

  if (speed == 0.0f) {  // off means idle
    hbridge_set_state(HBRIDGE_MODE_OFF, 0);
  }
  else{
    if (this->direction == fan::FAN_DIRECTION_FORWARD) {
      hbridge_set_state(HBRIDGE_MODE_DIRECTION_A, speed);
    } else {  // fan::FAN_DIRECTION_REVERSE
      hbridge_set_state(HBRIDGE_MODE_DIRECTION_B, speed);
    }
  }

  if (this->oscillating_ != nullptr){
    this->oscillating_->set_state(this->oscillating);
  }
}

}  // namespace hbridge
}  // namespace esphome
