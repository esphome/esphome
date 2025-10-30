#include "select_call.h"
#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

SelectCall &SelectCall::set_option(const std::string &option) {
  return with_operation(SELECT_OP_SET).with_option(option);
}

SelectCall &SelectCall::set_option(const char *option) { return with_operation(SELECT_OP_SET).with_option(option); }

SelectCall &SelectCall::set_index(size_t index) { return with_operation(SELECT_OP_SET_INDEX).with_index(index); }

SelectCall &SelectCall::select_next(bool cycle) { return with_operation(SELECT_OP_NEXT).with_cycle(cycle); }

SelectCall &SelectCall::select_previous(bool cycle) { return with_operation(SELECT_OP_PREVIOUS).with_cycle(cycle); }

SelectCall &SelectCall::select_first() { return with_operation(SELECT_OP_FIRST); }

SelectCall &SelectCall::select_last() { return with_operation(SELECT_OP_LAST); }

SelectCall &SelectCall::with_operation(SelectOperation operation) {
  this->operation_ = operation;
  return *this;
}

SelectCall &SelectCall::with_cycle(bool cycle) {
  this->cycle_ = cycle;
  return *this;
}

SelectCall &SelectCall::with_option(const std::string &option) { return this->with_option(option.c_str()); }

SelectCall &SelectCall::with_option(const char *option) {
  // Find the option index - this validates the option exists
  this->index_ = this->parent_->index_of(option);
  return *this;
}

SelectCall &SelectCall::with_index(size_t index) {
  this->index_ = index;
  return *this;
}

void SelectCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();
  const auto &traits = parent->traits;
  const auto &options = traits.get_options();

  if (this->operation_ == SELECT_OP_NONE) {
    ESP_LOGW(TAG, "'%s' - SelectCall performed without selecting an operation", name);
    return;
  }
  if (options.empty()) {
    ESP_LOGW(TAG, "'%s' - Cannot perform SelectCall, select has no options", name);
    return;
  }

  size_t target_index;

  if (this->operation_ == SELECT_OP_SET || this->operation_ == SELECT_OP_SET_INDEX) {
    if (this->operation_ == SELECT_OP_SET) {
      ESP_LOGD(TAG, "'%s' - Setting", name);
    }
    if (!this->index_.has_value()) {
      ESP_LOGW(TAG, "'%s' - No option value set for SelectCall", name);
      return;
    }
    if (this->index_.value() >= options.size()) {
      ESP_LOGW(TAG, "'%s' - Index value %zu out of bounds", name, this->index_.value());
      return;
    }
    target_index = this->index_.value();
  } else if (this->operation_ == SELECT_OP_FIRST) {
    target_index = 0;
  } else if (this->operation_ == SELECT_OP_LAST) {
    target_index = options.size() - 1;
  } else {  // SELECT_OP_NEXT or SELECT_OP_PREVIOUS
    auto cycle = this->cycle_;
    ESP_LOGD(TAG, "'%s' - Selecting %s, with%s cycling", name, this->operation_ == SELECT_OP_NEXT ? "next" : "previous",
             cycle ? "" : "out");
    if (!parent->has_state()) {
      target_index = this->operation_ == SELECT_OP_NEXT ? 0 : options.size() - 1;
    } else {
      // Use cached active_index_ instead of index_of() lookup
      auto index = parent->active_index_;
      auto size = options.size();
      if (cycle) {
        target_index = (size + index + (this->operation_ == SELECT_OP_NEXT ? +1 : -1)) % size;
      } else {
        if (this->operation_ == SELECT_OP_PREVIOUS && index > 0) {
          target_index = index - 1;
        } else if (this->operation_ == SELECT_OP_NEXT && index < options.size() - 1) {
          target_index = index + 1;
        } else {
          return;
        }
      }
    }
  }

  // All operations use indices, call control() by index to avoid string conversion
  ESP_LOGD(TAG, "'%s' - Set selected option to: %s", name, options[target_index]);
  parent->control(target_index);
}

}  // namespace select
}  // namespace esphome
