#include "select_call.h"
#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

SelectCall &SelectCall::set_option(const std::string &option) {
  return with_operation(SELECT_OP_SET).with_option(option);
}

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

SelectCall &SelectCall::with_option(const std::string &option) {
  this->option_ = option;
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
  auto options = traits.get_options();

  if (this->operation_ == SELECT_OP_NONE) {
    ESP_LOGW(TAG, "'%s' - SelectCall performed without selecting an operation", name);
    return;
  }
  if (options.empty()) {
    ESP_LOGW(TAG, "'%s' - Cannot perform SelectCall, select has no options", name);
    return;
  }

  std::string target_value;

  if (this->operation_ == SELECT_OP_SET) {
    ESP_LOGD(TAG, "'%s' - Setting", name);
    if (!this->option_.has_value()) {
      ESP_LOGW(TAG, "'%s' - No option value set for SelectCall", name);
      return;
    }
    target_value = this->option_.value();
  } else if (this->operation_ == SELECT_OP_SET_INDEX) {
    if (!this->index_.has_value()) {
      ESP_LOGW(TAG, "'%s' - No index value set for SelectCall", name);
      return;
    }
    if (this->index_.value() >= options.size()) {
      ESP_LOGW(TAG, "'%s' - Index value %d out of bounds", name, this->index_.value());
      return;
    }
    target_value = options[this->index_.value()];
  } else if (this->operation_ == SELECT_OP_FIRST) {
    target_value = options.front();
  } else if (this->operation_ == SELECT_OP_LAST) {
    target_value = options.back();
  } else if (this->operation_ == SELECT_OP_NEXT || this->operation_ == SELECT_OP_PREVIOUS) {
    auto cycle = this->cycle_;
    ESP_LOGD(TAG, "'%s' - Selecting %s, with%s cycling", name, this->operation_ == SELECT_OP_NEXT ? "next" : "previous",
             cycle ? "" : "out");
    if (!parent->has_state()) {
      target_value = this->operation_ == SELECT_OP_NEXT ? options.front() : options.back();
    } else {
      auto index = parent->index_of(parent->state);
      if (index.has_value()) {
        auto size = options.size();
        if (cycle) {
          auto use_index = (size + index.value() + (this->operation_ == SELECT_OP_NEXT ? +1 : -1)) % size;
          target_value = options[use_index];
        } else {
          if (this->operation_ == SELECT_OP_PREVIOUS && index.value() > 0) {
            target_value = options[index.value() - 1];
          } else if (this->operation_ == SELECT_OP_NEXT && index.value() < options.size() - 1) {
            target_value = options[index.value() + 1];
          } else {
            return;
          }
        }
      } else {
        target_value = this->operation_ == SELECT_OP_NEXT ? options.front() : options.back();
      }
    }
  }

  if (std::find(options.begin(), options.end(), target_value) == options.end()) {
    ESP_LOGW(TAG, "'%s' - Option %s is not a valid option", name, target_value.c_str());
    return;
  }

  ESP_LOGD(TAG, "'%s' - Set selected option to: %s", name, target_value.c_str());
  parent->control(target_value);
}

}  // namespace select
}  // namespace esphome
