#include "tt21100_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tt21100 {

static const char *const TAG = "tt21100.binary_sensor";

void TT21100Button::setup() {
  this->parent_->register_button_listener(this);
  this->publish_initial_state(false);
}

void TT21100Button::dump_config() {
  LOG_BINARY_SENSOR("", "TT21100 Button", this);
  ESP_LOGCONFIG(TAG, "  Index: %u", this->index_);
}

void TT21100Button::update_button(uint8_t index, uint16_t state) {
  if (index != this->index_)
    return;

  this->publish_state(state > 0);
}

}  // namespace tt21100
}  // namespace esphome
