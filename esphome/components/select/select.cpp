#include "select.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::select {

static const char *const TAG = "select";

void Select::publish_state(const std::string &state) { this->publish_state(state.c_str()); }

void Select::publish_state(const char *state) {
  auto index = this->index_of(state);
  if (index.has_value()) {
    this->publish_state(index.value());
  } else {
    ESP_LOGE(TAG, "'%s': Invalid option %s", this->get_name().c_str(), state);
  }
}

void Select::publish_state(size_t index) {
  if (!this->has_index(index)) {
    ESP_LOGE(TAG, "'%s': Invalid index %zu", this->get_name().c_str(), index);
    return;
  }
  const char *option = this->option_at(index);
  this->set_has_state(true);
  this->active_index_ = index;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  this->state = option;  // Update deprecated member for backward compatibility
#pragma GCC diagnostic pop
  ESP_LOGD(TAG, "'%s': Sending state %s (index %zu)", this->get_name().c_str(), option, index);
  // Callback signature requires std::string, create temporary for compatibility
  this->state_callback_.call(std::string(option), index);
#if defined(USE_SELECT) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_select_update(this);
#endif
}

const char *Select::current_option() const { return this->has_state() ? this->option_at(this->active_index_) : ""; }

void Select::add_on_state_callback(std::function<void(std::string, size_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

bool Select::has_option(const std::string &option) const { return this->index_of(option.c_str()).has_value(); }

bool Select::has_option(const char *option) const { return this->index_of(option).has_value(); }

bool Select::has_index(size_t index) const { return index < this->size(); }

size_t Select::size() const {
  const auto &options = traits.get_options();
  return options.size();
}

optional<size_t> Select::index_of(const char *option, size_t len) const {
  const auto &options = traits.get_options();
  for (size_t i = 0; i < options.size(); i++) {
    if (strncmp(options[i], option, len) == 0 && options[i][len] == '\0') {
      return i;
    }
  }
  return {};
}

optional<size_t> Select::active_index() const {
  if (this->has_state()) {
    return this->active_index_;
  }
  return {};
}

optional<std::string> Select::at(size_t index) const {
  if (this->has_index(index)) {
    const auto &options = traits.get_options();
    return std::string(options.at(index));
  }
  return {};
}

const char *Select::option_at(size_t index) const { return traits.get_options().at(index); }

}  // namespace esphome::select
