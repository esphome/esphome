#include "lt_component.h"

#ifdef USE_LIBRETUYA

#include "esphome/core/log.h"

namespace esphome {
namespace libretuya {

static const char *const TAG = "lt.component";

void LTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LibreTuya:");
  ESP_LOGCONFIG(TAG, "  Version: %s", LT.getVersion());
  ESP_LOGCONFIG(TAG, "  Loglevel: %u", LT_LOGLEVEL);

#ifdef USE_TEXT_SENSOR
  if (this->version_ != nullptr) {
    this->version_->publish_state(LT.getVersion());
  }
#endif  // USE_TEXT_SENSOR
}

void LTComponent::loop() {}

void LTComponent::update() {}

float LTComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace libretuya
}  // namespace esphome

#endif  // USE_LIBRETUYA
