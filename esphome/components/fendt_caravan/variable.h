#pragma once
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace esphome::fendt_caravan {
#define GET_VARIABLE(T, name) (static_cast<Variable<T> *>(this->get_variable(name)))

class IVariable {
 public:
  std::string get_name() { return this->name_; }
  void set_name(const std::string &name) { this->name_ = name; }
  std::string get_raw_value() { return this->raw_value_; }

  bool is_active() { return this->is_active_; }
  void is_active(bool active) { this->is_active_ = active; }
  virtual void decode(const std::string &value) = 0;

 protected:
  bool is_active_{false};
  std::string name_{};
  std::string raw_value_{};
};

template<typename T> class Variable : public IVariable {
 public:
  Variable(const std::string &name, std::function<T(const std::string &)> decode_func = nullptr,
           std::function<std::string(const std::string &, T)> command_func = nullptr,
           std::function<std::string(const std::string &, T)> alt_command_func = nullptr)
      : decode_funct_(std::move(decode_func)),
        command_funct_(std::move(command_func)),
        alt_command_funct_(std::move(alt_command_func)) {
    name_ = name;
  }

  void set_value(T value) { this->value_ = std::move(value); }
  T get_value() { return this->value_; }

  std::string get_command() {
    if (this->command_funct_) {
      return this->command_funct_(this->name_, this->value_);
    }
    return "";
  }

  std::string get_alt_command() {
    if (this->alt_command_funct_) {
      return this->alt_command_funct_(this->name_, this->value_);
    }
    return "";
  }

  void set_on_decode_callback(std::function<void(const T &value)> &&callback) {
    this->on_decode_.add(std::move(callback));
  }

  void decode(const std::string &value) override {
    this->raw_value_ = value;
    if (this->decode_funct_ == nullptr) {
      this->is_active_ = false;
      return;
    }
    this->value_ = this->decode_funct_(value);
    this->is_active_ = true;
    this->on_decode_.call(this->value_);
  }

 protected:
  std::function<T(const std::string &)> decode_funct_{};
  std::function<std::string(const std::string &, T)> command_funct_{};
  std::function<std::string(const std::string &, T)> alt_command_funct_{};
  CallbackManager<void(const T &value)> on_decode_{};
  T value_{};
};

}  // namespace esphome::fendt_caravan
