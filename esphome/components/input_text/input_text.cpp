#include "input_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace input_text {

static const char *const TAG = "input_text";

void InputText::publish_state(const std::string &state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGV(TAG, "'%s': Sending state %s", this->get_name().c_str(), state.c_str());
  this->state_callback_.call(state);
}

void InputText::add_on_state_callback(std::function<void(std::string)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace input_text
}  // namespace esphome
