#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

void SelectCall::perform() {
  auto parent = this->parent_;
  auto name = parent->get_name().c_str();
  const auto &traits = parent->traits;
  auto options = traits.get_options();

  if (this->op_ == SELECT_OP_NONE) {
    ESP_LOGW(TAG, "'%s': SelectCall performed without selecting an operation", name);
    return;
  }
  if (options.empty()) { 
    ESP_LOGW(TAG, "'%s': Cannot perform SelectCall, select has no options", name);
    return;
  }

  std::string target_value;
  
  if (this->op_ == SELECT_OP_SET) {
    ESP_LOGD(TAG, "'%s': Setting", name);
    if (!this->option_.has_value()) {
      ESP_LOGW(TAG, "'%s': No value set for SelectCall", name);
      return;
    }
    target_value = this->option_.value();
  }
  else if (this->op_ == SELECT_OP_FIRST) {
    target_value = options.front();
  }
  else if (this->op_ == SELECT_OP_LAST) {
    target_value = options.back();
  }
  else if (this->op_ == SELECT_OP_NEXT || this->op_ == SELECT_OP_PREVIOUS) {
    auto cycle = this->cycle_;
    ESP_LOGD(TAG, "'%s': Selecting %s, with%s cycling",
             name, this->op_ == SELECT_OP_NEXT ? "next" : "previous",
             cycle ? "" : "out");
    if (!parent->has_state()) {
      target_value = this->op_ == SELECT_OP_NEXT ? options.front() : options.back();
    }
    else {
      auto index = parent->index_of(parent->state);
      if (index.has_value()) {
        auto size = options.size();
        if (cycle) {
          auto use_index = (size + index.value() + (this->op_ == SELECT_OP_NEXT ? +1 : -1)) % size;
          target_value = options[use_index];
        }
        else {
          if (this->op_ == SELECT_OP_PREVIOUS && index.value() > 0) {
            target_value = options[index.value() - 1]; 
          }
          else if (this->op_ == SELECT_OP_NEXT && index.value() < options.size() - 1) {
            target_value = options[index.value() + 1]; 
          }
          else {
            return;
          }
        }
      } else {
        target_value = this->op_ == SELECT_OP_NEXT ? options.front() : options.back();
      }
    }
  }

  if (std::find(options.begin(), options.end(), target_value) == options.end()) {
    ESP_LOGW(TAG, "'%s': Option %s is not a valid option.", name, target_value.c_str());
    return;
  }

  ESP_LOGD(TAG, "'%s': Set selected option to: %s", name, target_value.c_str());
  parent->control(target_value);
}

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
