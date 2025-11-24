#include "st7123_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7123 {

static const char *const TAG = "ST7123.binary_sensor";

void ST7123Button::setup() {
  this->parent_->register_button_listener(this);
  this->publish_initial_state(false);
}

void ST7123Button::dump_config() {
  LOG_BINARY_SENSOR("", "ST7123 Button", this);
  ESP_LOGCONFIG(TAG, "  Index: %u", this->index_);
}

void ST7123Button::update_button(uint8_t index, bool state) {
  if (index != this->index_)
    return;

  this->publish_state(state);
}

}  // namespace st7123
}  // namespace esphome
