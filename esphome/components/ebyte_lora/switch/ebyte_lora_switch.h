#pragma once
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../ebyte_lora.h"

namespace esphome {
namespace ebyte_lora {
class EbyteLoraSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
  void set_parent(EbyteLoraComponent *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }

 protected:
  void write_state(bool state) override;
  EbyteLoraComponent *parent_;
  uint8_t pin_;
};
}  // namespace ebyte_lora
}  // namespace esphome
