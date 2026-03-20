#include "update_entity.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome {
namespace update {

static const char *const TAG = "update";

// Update state strings indexed by UpdateState enum (0-3): UNKNOWN, NO UPDATE, UPDATE AVAILABLE, INSTALLING
PROGMEM_STRING_TABLE(UpdateStateStrings, "UNKNOWN", "NO UPDATE", "UPDATE AVAILABLE", "INSTALLING");

const LogString *update_state_to_string(UpdateState state) {
  return UpdateStateStrings::get_log_str(static_cast<uint8_t>(state),
                                         static_cast<uint8_t>(UpdateState::UPDATE_STATE_UNKNOWN));
}

void UpdateEntity::publish_state() {
  ESP_LOGD(TAG,
           "'%s' >>\n"
           "  Current Version: %s",
           this->name_.c_str(), this->update_info_.current_version.c_str());

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

  this->set_has_state(true);
  this->state_callback_.call();
#if defined(USE_UPDATE) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_update(this);
#endif
}

}  // namespace update
}  // namespace esphome
