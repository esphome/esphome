#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace lvgl {

static std::vector<std::string> split_string(const std::string &str) {
  std::vector<std::string> strings;
  auto delimiter = std::string("\n");

  std::string::size_type pos;
  std::string::size_type prev = 0;
  while ((pos = str.find(delimiter, prev)) != std::string::npos) {
    strings.push_back(str.substr(prev, pos - prev));
    prev = pos + delimiter.size();
  }

  // To get the last substring (or only, if delimiter is not found)
  strings.push_back(str.substr(prev));

  return strings;
}

class LVGLSelect : public select::Select {
 public:
  void set_control_lambda(std::function<void(size_t)> lambda) {
    this->control_lambda_ = lambda;
    if (this->initial_state_.has_value()) {
      this->control(this->initial_state_.value());
      this->initial_state_.reset();
    }
  }

  void publish_index(size_t index) {
    auto value = this->at(index);
    if (value)
      this->publish_state(value.value());
  }

  void set_options(const char *str) { this->traits.set_options(split_string(str)); }

 protected:
  void control(const std::string &value) override {
    if (this->control_lambda_ != nullptr) {
      auto index = index_of(value);
      if (index)
        this->control_lambda_(index.value());
    } else {
      this->initial_state_ = value.c_str();
    }
  }

  std::function<void(size_t)> control_lambda_{};
  optional<const char *> initial_state_{};
};

}  // namespace lvgl
}  // namespace esphome
