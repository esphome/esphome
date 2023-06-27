#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

void Select::publish_state(const std::string &state) {
  auto index = this->index_of(state);
  const auto *name = this->get_name().c_str();
  if (index.has_value()) {
    this->has_state_ = true;
    this->state = state;
    ESP_LOGD(TAG, "'%s': Sending state %s (index %d)", name, state.c_str(), index.value());
    this->state_callback_.call(state, index.value());
  } else {
    ESP_LOGE(TAG, "'%s': invalid state for publish_state(): %s", name, state.c_str());
  }
}

void Select::add_on_state_callback(std::function<void(std::string, size_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

bool Select::has_option(const std::string &option) const { return this->index_of(option).has_value(); }

bool Select::has_index(size_t index) const { return index < this->size(); }

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
  if (this->has_index(index)) {
    auto options = traits.get_options();
    return options.at(index);
  } else {
    return {};
  }
}

}  // namespace select
}  // namespace esphome
