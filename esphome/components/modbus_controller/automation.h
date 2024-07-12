#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/modbus_controller/modbus_controller.h"

namespace esphome {
namespace modbus_controller {

class ModbusWriteTrigger : public Trigger<> {
 public:
  ModbusWriteTrigger(ModbusController *a_modbuscontroller) {
    a_modbuscontroller->add_on_write_callback([this]() {
        this->trigger();
    });
  }
};

} //namespace modbus_controller
} //namespace esphome