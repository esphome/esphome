#include "c4004_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_c4004 {

static const char *const TAG = "dfrobot_c4004.button";

void C4004FactoryResetButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->factory_reset()) {
    ESP_LOGW(TAG, "Factory reset command failed");
  }
}

void C4004ResetButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->reset_device()) {
    ESP_LOGW(TAG, "Reset command failed");
  }
}

void C4004SaveInstallSettingsButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->save_install_settings()) {
    ESP_LOGW(TAG, "Save install settings command failed");
  }
}

void C4004ApplyBoundaryRangeButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->apply_boundary_range()) {
    ESP_LOGW(TAG, "Apply boundary range command failed");
  }
}

void C4004SetTrajectoryRangeModeButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->set_trajectory_range_mode()) {
    ESP_LOGW(TAG, "Set trajectory range mode command failed");
  }
}

void C4004ClearAllTagsButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->clear_all_tags()) {
    ESP_LOGW(TAG, "Clear all tags command failed");
  }
}

void C4004ClearPeopleCountButton::press_action() {
  if (this->parent_ != nullptr && !this->parent_->clear_people_count_command()) {
    ESP_LOGW(TAG, "Clear people count command failed");
  }
}

}  // namespace dfrobot_c4004
}  // namespace esphome
