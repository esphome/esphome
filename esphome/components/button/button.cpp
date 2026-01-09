#include "button.h"
#include "esphome/core/log.h"

namespace esphome::button {

static const char *const TAG = "button";

// Function implementation of LOG_BUTTON macro to reduce code size
void log_button(const char *tag, const char *prefix, const char *type, Button *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());

  if (!obj->get_icon_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", prefix, obj->get_icon_ref().c_str());
  }
}

void Button::press() {
  ESP_LOGD(TAG, "'%s' Pressed.", this->get_name().c_str());
  this->press_action();
  this->press_callback_.call();
}
void Button::add_on_press_callback(std::function<void()> &&callback) { this->press_callback_.add(std::move(callback)); }

}  // namespace esphome::button
