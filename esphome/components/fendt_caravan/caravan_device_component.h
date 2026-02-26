#pragma once

#ifdef USE_ESP32
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "variable.h"

namespace esphome::fendt_caravan {

class CaravanDeviceComponent : public Component {
 public:
  virtual bool decode(const std::string &name, const std::string &value) {
    auto *variable = this->get_variable(name);
    if (variable) {
      variable->decode(value);
      on_data_decoded(variable);
    }
    return variable != nullptr;
  }
  virtual void on_state_change_command(const std::string &tag, const std::string &command) = 0;
  void add_variable(IVariable *variable) { this->variables_.push_back(variable); }
  void set_command_send_callback(std::function<void(const std::string &)> &&callback) {
    this->command_callback_.add(std::move(callback));
  }
  void setup() override{};
  void loop() override{};
  void dump_config() override = 0;

  IVariable *get_variable(const std::string &name) {
    for (auto *variable : this->variables_) {
      if (variable->get_name() == name)
        return variable;
    }
    return nullptr;
  }

 protected:
  virtual void on_data_decoded(IVariable *variable){};
  std::vector<IVariable *> variables_{};
  CallbackManager<void(const std::string &)> command_callback_{};
  std::vector<IVariable *> get_variables_() { return this->variables_; }

 private:
};

}  // namespace esphome::fendt_caravan
#endif
