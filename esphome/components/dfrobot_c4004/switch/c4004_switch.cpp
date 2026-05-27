#include "c4004_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4004 {

static const char *const TAG = "dfrobot_c4004.switch";

void C4004PresenceEnableSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_presence_enable(state)) {
    this->publish_state(state);
  } else {
    ESP_LOGW(TAG, "Failed to set presence enable");
    this->publish_state(!state);
  }
}

void C4004TrajectoryTrackingSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_trajectory_tracking(state)) {
    this->publish_state(state);
  } else {
    ESP_LOGW(TAG, "Failed to set trajectory tracking");
    this->publish_state(!state);
  }
}

void C4004TrajectoryLedSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_trajectory_led(state)) {
    this->publish_state(state);
  } else {
    ESP_LOGW(TAG, "Failed to set trajectory LED");
    this->publish_state(!state);
  }
}

void C4004MotionLedSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_motion_led(state)) {
    this->publish_state(state);
  } else {
    ESP_LOGW(TAG, "Failed to set motion LED");
    this->publish_state(!state);
  }
}

}  // namespace dfrobot_c4004
}  // namespace esphome
