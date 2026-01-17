#pragma once
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "variable.h"
#include "device_commands.h"
#include "device_decoders.h"

namespace esphome {
namespace fendt_caravan {
using namespace std;

class CaravanDevice : public PollingComponent {
 public:
  virtual void decode(const std::string &name, const std::string &value);
  void add_variable(IVariable *variable) { this->variables_.push_back(variable); }
  void set_command_send_callback(std::function<void(const std::string &)> &&callback) {
    this->command_callback_.add(std::move(callback));
  }
  void setup() override{};
  void loop() override{};
  void update() override;
  void dump_config() override = 0;

 protected:
  std::vector<IVariable *> variables_{};
  CallbackManager<void(const std::string &)> command_callback_{};
  std::vector<IVariable *> get_variables_() { return this->variables_; }
  IVariable *get_variable_(const std::string &name) {
    for (auto *variable : this->variables_) {
      if (variable->get_name() == name)
        return variable;
    }
    return nullptr;
  }
  virtual const char *get_tag() = 0;

 private:
  bool log_variables_ = false;
};

}  // namespace fendt_caravan
}  // namespace esphome
