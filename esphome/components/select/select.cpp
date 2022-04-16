#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

void Select::publish_state(const std::string &state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %s", this->get_name().c_str(), state.c_str());
  this->state_callback_.call(state);
}

void Select::add_on_state_callback(std::function<void(std::string)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

size_t Select::number_of_options() const {
  auto options = traits.get_options();
  return options.size();
}

optional<size_t> Select::index_of(const std::string &option) const {
  auto options = traits.get_options();
  auto it = std::find(options.begin(), options.end(), option);
  if (it == options.end()) {
    return {};
  }
  return std::distance(options.begin(), it);
}

optional<std::string> Select::option_at(const size_t index) const {
  auto options = traits.get_options();
  if (index >= options.size()) {
    return {};
  }
  return options[index];
}

uint32_t Select::hash_base() { return 2812997003UL; }

}  // namespace select
}  // namespace esphome
