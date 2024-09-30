#include "update_entity.h"

#include "esphome/core/log.h"

namespace esphome {
namespace update {

static const char *const TAG = "update";

void UpdateEntity::publish_state() {
  ESP_LOGD(TAG, "'%s' - Publishing:", this->name_.c_str());
  ESP_LOGD(TAG, "  Current Version: %s", this->update_info_.current_version.c_str());

  if (!this->update_info_.md5.empty()) {
    ESP_LOGD(TAG, "  Latest Version: %s", this->update_info_.latest_version.c_str());
  }
  if (!this->update_info_.firmware_url.empty()) {
    ESP_LOGD(TAG, "  Firmware URL: %s", this->update_info_.firmware_url.c_str());
  }

  ESP_LOGD(TAG, "  Title: %s", this->update_info_.title.c_str());
  if (!this->update_info_.summary.empty()) {
    ESP_LOGD(TAG, "  Summary: %s", this->update_info_.summary.c_str());
  }
  if (!this->update_info_.release_url.empty()) {
    ESP_LOGD(TAG, "  Release URL: %s", this->update_info_.release_url.c_str());
  }

  if (this->update_info_.has_progress) {
    ESP_LOGD(TAG, "  Progress: %.0f%%", this->update_info_.progress);
  }

  this->has_state_ = true;
  this->state_callback_.call();
}

}  // namespace update
}  // namespace esphome
