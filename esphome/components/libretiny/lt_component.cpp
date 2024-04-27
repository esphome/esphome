#include "lt_component.h"

#ifdef USE_LIBRETINY

#include "esphome/core/log.h"

namespace esphome {
namespace libretiny {

static const char *const TAG = "lt.component";

void LTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LibreTiny:");
  ESP_LOGCONFIG(TAG, "  Version: %s", LT_BANNER_STR + 10);
  ESP_LOGCONFIG(TAG, "  Loglevel: %u", LT_LOGLEVEL);

#ifdef USE_TEXT_SENSOR
  if (this->version_ != nullptr) {
    this->version_->publish_state(LT_BANNER_STR + 10);
  }
#endif  // USE_TEXT_SENSOR
}

float LTComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace libretiny
}  // namespace esphome

#endif  // USE_LIBRETINY
