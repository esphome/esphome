#ifdef USE_ESP32

#include "ecocomfort2_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.button";

void Ecocomfort2PairButton::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Pair Button:");
  LOG_BUTTON("  ", "Pair", this);
}

void Ecocomfort2PairButton::press_action() {
  if (!this->parent_->is_connected()) {
    ESP_LOGW(TAG, "Not connected, cannot pair");
    return;
  }

  ESP_LOGI(TAG, "Pairing with VMC unit...");
  this->parent_->pair();
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
