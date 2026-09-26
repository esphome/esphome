#pragma once

#ifdef USE_ESP32
#include "esphome/core/string_ref.h"
#include "esphome/core/component.h"
#include "variable.h"

namespace esphome::fendt_caravan {

class FendtCaravanHubBase : public PollingComponent {
 public:
  virtual bool decode(const std::string &name, const std::string &value) {
    auto *variable = this->get_variable(name);
    if (variable) {
      variable->decode(value);
    }
    return variable != nullptr;
  }
  void add_variable(IVariable *variable) { this->variables_.push_back(variable); }
  void setup() override{};
  void loop() override{};
  void dump_config() override = 0;
  void update() override{};

  IVariable *get_variable(const std::string &name) {
    for (auto *variable : this->variables_) {
      if (variable->get_name() == name)
        return variable;
    }
    return nullptr;
  }

 protected:
  std::vector<IVariable *> variables_{};
  std::vector<IVariable *> get_variables_() { return this->variables_; }

 private:
};

}  // namespace esphome::fendt_caravan
#endif
