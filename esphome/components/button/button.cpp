#include "button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace button {

static const char *const TAG = "button";

void Button::press() {
  ESP_LOGD(TAG, "'%s' Pressed.", this->get_name().c_str());
  this->press_action();
  this->press_callback_.call();
}
void Button::add_on_press_callback(std::function<void()> &&callback) { this->press_callback_.add(std::move(callback)); }

}  // namespace button
}  // namespace esphome
