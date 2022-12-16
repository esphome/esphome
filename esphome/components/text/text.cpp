#include "text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace text {

static const char *const TAG = "text";

void Text::publish_state(const std::string &state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGV(TAG, "'%s': Sending state %s", this->get_name().c_str(), state.c_str());
  this->state_callback_.call(state);
}

void Text::add_on_state_callback(std::function<void(std::string)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace text
}  // namespace esphome
