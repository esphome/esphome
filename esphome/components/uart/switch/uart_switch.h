#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace uart {

class UARTSwitch : public switch_::Switch, public UARTDevice, public Component {
 public:
  UARTSwitch(const std::string &name, const std::vector<uint8_t> &data);

  void dump_config() override;

 protected:
  void write_state(bool state) override;
  std::vector<uint8_t> data_;
};

}  // namespace uart
}  // namespace esphome
