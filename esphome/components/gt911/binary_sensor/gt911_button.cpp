#include "gt911_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gt911 {

static const char *const TAG = "GT911.binary_sensor";

void GT911Button::setup() {
  this->parent_->register_button_listener(this);
  this->publish_initial_state(false);
}

void GT911Button::dump_config() {
  LOG_BINARY_SENSOR("", "GT911 Button", this);
  ESP_LOGCONFIG(TAG, "  Index: %u", this->index_);
}

void GT911Button::update_button(uint8_t index, bool state) {
  if (index != this->index_)
    return;

  this->publish_state(state);
}

}  // namespace gt911
}  // namespace esphome
