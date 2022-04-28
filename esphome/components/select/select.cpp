#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

void Select::publish_state(const std::string &state) {
  this->has_state_ = true;
  this->state = state;
  size_t index = this->index_of(state).value();
  ESP_LOGD(TAG, "'%s': Sending state %s (index %d)", this->get_name().c_str(), state.c_str(), index);
  this->state_callback_.call(state, index);
}

void Select::add_on_state_callback(std::function<void(std::string, size_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

size_t Select::size() const {
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

optional<size_t> Select::active_index() const {
  if (this->has_state()) {
    return this->index_of(this->state);
  } else {
    return {};
  }
}

optional<std::string> Select::at(size_t index) const {
  auto options = traits.get_options();
  if (index >= options.size()) {
    return {};
  }
  return options.at(index);
}

uint32_t Select::hash_base() { return 2812997003UL; }

}  // namespace select
}  // namespace esphome
