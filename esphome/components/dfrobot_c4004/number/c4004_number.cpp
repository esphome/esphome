#include "c4004_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4004 {

static const char *const TAG = "dfrobot_c4004.number";

void C4004InstallHeightNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_pending_install_height(value);
  this->publish_state(value);
}

void C4004InstallZAngleNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_pending_install_z_angle(value);
  this->publish_state(value);
}

void C4004RangeXMaxNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_pending_range_x_max(value);
  this->publish_state(value);
}

void C4004RangeXMinNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_pending_range_x_min(value);
  this->publish_state(value);
}

void C4004RangeYMaxNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_pending_range_y_max(value);
  this->publish_state(value);
}

void C4004RangeYMinNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_pending_range_y_min(value);
  this->publish_state(value);
}

void C4004TargetCountNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  ESP_LOGW(TAG, "Target count is read-only in the C4004 protocol; requested %.0f is ignored", value);
  this->parent_->publish_target_count_number();
}

void C4004PeopleReportIntervalNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_people_report_interval(value)) {
    this->publish_state(value);
  } else {
    ESP_LOGW(TAG, "Failed to set people report interval");
    this->publish_state(this->parent_->get_people_report_interval());
  }
}

void C4004TrajectoryGenerateDistanceNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_trajectory_generate_distance(value)) {
    this->publish_state(value);
  } else {
    ESP_LOGW(TAG, "Failed to set trajectory generate distance");
    this->publish_state(this->parent_->get_trajectory_generate_distance());
  }
}

void C4004TrajectoryHoldTimeNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_trajectory_hold_time(value)) {
    this->publish_state(value);
  } else {
    ESP_LOGW(TAG, "Failed to set trajectory hold time");
    this->publish_state(this->parent_->get_trajectory_hold_time());
  }
}

void C4004NoPersonDelayNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->write_no_person_delay(value)) {
    this->publish_state(value);
  } else {
    ESP_LOGW(TAG, "Failed to set no-person delay");
    this->publish_state(this->parent_->get_no_person_delay());
  }
}

}  // namespace dfrobot_c4004
}  // namespace esphome
