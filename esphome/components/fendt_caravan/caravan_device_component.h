#pragma once

#ifdef USE_ESP32
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "variable.h"

namespace esphome::fendt_caravan {
using namespace std;

enum DeviceType : uint8_t {
  DEVICE_TYPE_NONE = 0,
  DEVICE_TYPE_MCU,
};

class CaravanDeviceComponent : public Component {
 public:
  virtual bool decode(const std::string &name, const std::string &value) {
    auto *variable = this->get_variable_(name);
    if (variable)
      variable->decode(value);
    return variable != nullptr;
  }
  virtual void on_switch_state_change_(switch_::Switch *sw, bool state) = 0;
  void add_variable(IVariable *variable) { this->variables_.push_back(variable); }
  void set_command_send_callback(std::function<void(const std::string &)> &&callback) {
    this->command_callback_.add(std::move(callback));
  }
  void setup() override{};
  void loop() override{};
  void dump_config() override = 0;

 protected:
  std::vector<IVariable *> variables_{};
  CallbackManager<void(const std::string &)> command_callback_{};
  std::vector<IVariable *> get_variables_() { return this->variables_; }
  virtual DeviceType get_device_type_() = 0;
  IVariable *get_variable_(const std::string &name) {
    for (auto *variable : this->variables_) {
      if (variable->get_name() == name)
        return variable;
    }
    return nullptr;
  }

 private:
  bool log_variables_ = true;
  DeviceType dvice_type_ = DeviceType::DEVICE_TYPE_NONE;
};

}  // namespace esphome::fendt_caravan
#endif
