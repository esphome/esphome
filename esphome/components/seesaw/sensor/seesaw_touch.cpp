#include "seesaw_touch.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.touch");

void SeesawTouch::dump_config() {
  LOG_SENSOR("", "Seesaw Touch", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_UPDATE_INTERVAL(this);
}

void SeesawTouch::update() {
  int16_t value = this->parent_->get_touch_value(this->channel_);
  if (value == -1) {
    ESP_LOGW(TAG, "touch reading failed for channel %d", this->channel_);
  } else {
    this->publish_state(value);
  }
}

}  // namespace esphome::seesaw
