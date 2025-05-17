#include "cs226_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst226 {

static const char *const TAG = "CST226.binary_sensor";

void CST226Button::setup() {
  this->parent_->register_button_listener(this);
  this->publish_initial_state(false);
}

void CST226Button::dump_config() { LOG_BINARY_SENSOR("", "CST226 Button", this); }

void CST226Button::update_button(bool state) { this->publish_state(state); }

}  // namespace cst226
}  // namespace esphome
