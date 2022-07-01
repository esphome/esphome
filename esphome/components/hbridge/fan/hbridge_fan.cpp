#include "hbridge_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "fan.hbridge";
// Let H-Bridge base component use our log tag
const char *HBridgeFan::get_log_tag() { return TAG; }

fan::FanCall HBridgeFan::brake() {
  HBridge::transition_to_state(HBridgeMode::SHORT, 1, HBridge::setting_full_rampup_time_ms_,
                               HBridge::setting_full_rampdown_time_ms_, HBridge::setting_short_buildup_time_ms_,
                               HBridge::setting_short_time_ms_);
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
  float speed_dutycycle = this->state ? static_cast<float>(this->speed) / static_cast<float>(this->speed_count_) : 0.0f;

  // Set speed/direction
  if (speed_dutycycle == 0.0f) {  // speed_dutycycle 0 means off
    HBridge::transition_to_state(HBridgeMode::OFF, 0, HBridge::setting_full_rampup_time_ms_,
                                 HBridge::setting_full_rampdown_time_ms_, HBridge::setting_short_buildup_time_ms_,
                                 HBridge::setting_short_time_ms_);
  } else {
    if (this->direction == fan::FanDirection::FORWARD) {
      HBridge::transition_to_state(HBridgeMode::DIRECTION_A, speed_dutycycle, HBridge::setting_full_rampup_time_ms_,
                                   HBridge::setting_full_rampdown_time_ms_, HBridge::setting_short_buildup_time_ms_,
                                   HBridge::setting_short_time_ms_);
    } else {  // fan::FanDirection::REVERSE
      HBridge::transition_to_state(HBridgeMode::DIRECTION_B, speed_dutycycle, HBridge::setting_full_rampup_time_ms_,
                                   HBridge::setting_full_rampdown_time_ms_, HBridge::setting_short_buildup_time_ms_,
                                   HBridge::setting_short_time_ms_);
    }
  }

  if (this->oscillating_ != nullptr) {
    this->oscillating_->set_state(this->oscillating);
  }
}

}  // namespace hbridge
}  // namespace esphome
