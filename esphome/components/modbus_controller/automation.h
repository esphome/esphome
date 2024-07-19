#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/modbus_controller/modbus_controller.h"

namespace esphome {
namespace modbus_controller {

class ModbusCommandSentTrigger : public Trigger<int, int> {
 public:
  ModbusCommandSentTrigger(ModbusController *a_modbuscontroller) {
    a_modbuscontroller->add_on_command_sent_callback(
        [this](int function_code, int address) { this->trigger(function_code, address); });
  }
};

}  // namespace modbus_controller
}  // namespace esphome
