#pragma once

#ifdef USE_ESP32
#include "variable.h"
#include "esphome/core/component.h"
#include "caravan_device_component.h"

namespace esphome::fendt_caravan {

template<typename T> class CaravanComponentBase : public Component, public Parented<CaravanDeviceComponent> {
 public:
  void set_variable(Variable<T> *variable) {
    this->variable_ = variable;
    this->variable_->set_on_decode_callback([this](auto const &arg) { this->on_decoded(std::move(arg)); });
  }

  Variable<T> *get_variable() { return this->variable_; }

  Variable<T> *create_variable(
      const std::string &name, std::function<T(const std::string &)> decode_funct,
      std::function<std::string(const std::string &name, T value)> command_funct = nullptr,
      std::function<std::string(const std::string &name, T value)> alt_command_funct = nullptr) {
    auto variable = new Variable<T>(name, decode_funct, command_funct, alt_command_funct);
    this->set_variable(variable);
    return variable;
  }
  void set_key_name(std::string key_name) { this->key_name_ = std::move(key_name); }

  void setup() override = 0;

 protected:
  virtual void on_decoded(const T &value) {}
  Variable<T> *variable_{nullptr};
  std::string key_name_;
};

}  // namespace esphome::fendt_caravan
#endif
