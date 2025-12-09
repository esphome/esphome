#include "select_call.h"
#include "select.h"
#include "esphome/core/log.h"

namespace esphome::select {

static const char *const TAG = "select";

SelectCall &SelectCall::set_option(const char *option, size_t len) { return this->with_option(option, len); }

SelectCall &SelectCall::set_index(size_t index) { return this->with_index(index); }

SelectCall &SelectCall::select_next(bool cycle) { return this->with_operation(SELECT_OP_NEXT).with_cycle(cycle); }

SelectCall &SelectCall::select_previous(bool cycle) {
  return this->with_operation(SELECT_OP_PREVIOUS).with_cycle(cycle);
}

SelectCall &SelectCall::select_first() { return this->with_operation(SELECT_OP_FIRST); }

SelectCall &SelectCall::select_last() { return this->with_operation(SELECT_OP_LAST); }

SelectCall &SelectCall::with_operation(SelectOperation operation) {
  this->operation_ = operation;
  return *this;
}

SelectCall &SelectCall::with_cycle(bool cycle) {
  this->cycle_ = cycle;
  return *this;
}

SelectCall &SelectCall::with_option(const char *option, size_t len) {
  this->operation_ = SELECT_OP_SET;
  // Find the option index - this validates the option exists
  this->index_ = this->parent_->index_of(option, len);
  return *this;
}

SelectCall &SelectCall::with_index(size_t index) {
  this->operation_ = SELECT_OP_SET;
  if (index >= this->parent_->size()) {
    ESP_LOGW(TAG, "'%s' - Index value %zu out of bounds", this->parent_->get_name().c_str(), index);
    this->index_ = {};  // Store nullopt for invalid index
  } else {
    this->index_ = index;
  }
  return *this;
}

optional<size_t> SelectCall::calculate_target_index_(const char *name) {
  const auto &options = this->parent_->traits.get_options();
  if (options.empty()) {
    ESP_LOGW(TAG, "'%s' - Select has no options", name);
    return {};
  }

  if (this->operation_ == SELECT_OP_FIRST) {
    return 0;
  }

  if (this->operation_ == SELECT_OP_LAST) {
    return options.size() - 1;
  }

  if (this->operation_ == SELECT_OP_SET) {
    ESP_LOGD(TAG, "'%s' - Setting", name);
    if (!this->index_.has_value()) {
      ESP_LOGW(TAG, "'%s' - No option set", name);
      return {};
    }
    return this->index_.value();
  }

  // SELECT_OP_NEXT or SELECT_OP_PREVIOUS
  ESP_LOGD(TAG, "'%s' - Selecting %s, with%s cycling", name,
           this->operation_ == SELECT_OP_NEXT ? LOG_STR_LITERAL("next") : LOG_STR_LITERAL("previous"),
           this->cycle_ ? LOG_STR_LITERAL("") : LOG_STR_LITERAL("out"));

  const auto size = options.size();
  if (!this->parent_->has_state()) {
    return this->operation_ == SELECT_OP_NEXT ? 0 : size - 1;
  }

  // Use cached active_index_ instead of index_of() lookup
  const auto active_index = this->parent_->active_index_;
  if (this->cycle_) {
    return (size + active_index + (this->operation_ == SELECT_OP_NEXT ? +1 : -1)) % size;
  }

  if (this->operation_ == SELECT_OP_PREVIOUS && active_index > 0) {
    return active_index - 1;
  }

  if (this->operation_ == SELECT_OP_NEXT && active_index < size - 1) {
    return active_index + 1;
  }

  return {};  // Can't navigate further without cycling
}

void SelectCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();

  if (this->operation_ == SELECT_OP_NONE) {
    ESP_LOGW(TAG, "'%s' - SelectCall performed without selecting an operation", name);
    return;
  }

  // Calculate target index (with_index() and with_option() already validate bounds/existence)
  auto target_index = this->calculate_target_index_(name);
  if (!target_index.has_value()) {
    return;
  }

  auto idx = target_index.value();
  // All operations use indices, call control() by index to avoid string conversion
  ESP_LOGD(TAG, "'%s' - Set selected option to: %s", name, parent->option_at(idx));
  parent->control(idx);
}

}  // namespace esphome::select
