#include "button.h"
#include "esphome/core/log.h"

namespace esphome::button {

ESPHOME_LOG_TAG(TAG, "button");

// Function implementation of LOG_BUTTON macro to reduce code size
void log_button(const char *tag, const char *prefix, const char *type, Button *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, LOG_STR_ARG(obj->get_log_name()));
  LOG_ENTITY_ICON(tag, prefix, *obj);
}

void Button::press() {
  ESP_LOGV(TAG, "'%s' Pressed.", LOG_STR_ARG(this->get_log_name()));
  this->press_action();
  this->press_callback_.call();
}

}  // namespace esphome::button
