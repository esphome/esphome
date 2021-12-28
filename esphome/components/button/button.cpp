#include "button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace button {

static const char *const TAG = "button";

Button::Button(const std::string &name) : EntityBase(name) {}
Button::Button() : Button("") {}

void Button::press() {
  ESP_LOGD(TAG, "'%s' Pressed.", this->get_name().c_str());

  auto state = this->generate_state();
  this->set_state(state);
}
void Button::set_state(const std::string &state) {
  if (state.empty()) {
    // No state, old Home Assistant version
    auto generated = this->generate_state();
    this->set_state(generated);
    return;
  }

  // Break possible endless loop when pressing the button in ESPHome changes the state
  // in Home Assistant which in turn sends the new state to ESPHome.
  if (this->state == state) {
    return;
  }

  ESP_LOGD(TAG, "'%s' State: %s", this->get_name().c_str(), state.c_str());

  this->state = state;
  this->press_action();
  this->press_callback_.call(state);
}
void Button::add_on_press_callback(std::function<void(std::string)> &&callback) {
  ESP_LOGD(TAG, "add_on_press_callback");
  this->press_callback_.add(std::move(callback));
}
uint32_t Button::hash_base() { return 1495763804UL; }

void Button::set_device_class(const std::string &device_class) { this->device_class_ = device_class; }
std::string Button::get_device_class() {
  if (this->device_class_.has_value())
    return *this->device_class_;
  return "";
}

std::string Button::generate_state() {
  std::string state;

#ifdef USE_TIME
  if (this->time_id_.has_value()) {
    auto now = this->time_id_.value()->utcnow();
    if (now.is_valid()) {
      return now.strftime("%Y-%m-%dT%H:%M:%S");
    } else {
      ESP_LOGW(TAG, "Time is not valid.");
    }
  }
#endif

  return to_string(random_uint32());
}

}  // namespace button
}  // namespace esphome
