#include "button.h"
#include "esphome/core/log.h"

namespace esphome::button {

ESPHOME_LOG_TAG(TAG, "button");

// Function implementation of LOG_BUTTON macro to reduce code size
void log_button(const char *tag, const char *prefix, const char *type, Button *obj) {
  if (!log_entity_header(tag, prefix, type, obj)) {
    return;
  }
  LOG_ENTITY_ICON(tag, prefix, *obj);
}

void Button::press() {
  ESP_LOGV(TAG, "'%s' Pressed.", this->get_name().c_str());
  this->press_action();
  this->press_callback_.call();
}

}  // namespace esphome::button
