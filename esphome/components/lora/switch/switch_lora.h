#pragma once
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../lora.h"

namespace esphome {
namespace lora {
class LoraSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
  void set_parent(Lora *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }

 protected:
  void write_state(bool state) override;
  Lora *parent_;
  uint8_t pin_;
};
}  // namespace lora
}  // namespace esphome
