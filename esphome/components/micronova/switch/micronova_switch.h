#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace micronova {

class MicroNovaSwitch : public Component, public switch_::Switch, public MicroNovaSwitchListener {
 public:
  MicroNovaSwitch(MicroNova *m) : MicroNovaSwitchListener(m) {}
  void dump_config() override { LOG_SWITCH("", "Micronova switch", this); }

  void set_stove_state(bool v) override { this->publish_state(v); }
  bool get_stove_state() override { return this->state; }

  void set_memory_data_on(uint8_t f) { this->memory_data_on_ = f; }
  uint8_t get_memory_data_on() { return this->memory_data_on_; }

  void set_memory_data_off(uint8_t f) { this->memory_data_off_ = f; }
  uint8_t get_memory_data_off() { return this->memory_data_off_; }

 protected:
  void write_state(bool state) override;
};

}  // namespace micronova
}  // namespace esphome
