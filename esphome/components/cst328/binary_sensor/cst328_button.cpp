#include "cst328_button.h"
#include "esphome/core/log.h"

namespace esphome::cst328 {
static const char *const TAG = "cst328.binary_sensor";

void CST328Button::setup() {
  this->parent_->register_button_listener(this);
  this->publish_initial_state(false);
}

void CST328Button::dump_config() { LOG_BINARY_SENSOR("", "CST328 Button", this); }

void CST328Button::update_button(bool state) { this->publish_state(state); }

}  // namespace esphome::cst328
