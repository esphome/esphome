#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome::micronova {

class MicroNovaSwitch : public Component, public switch_::Switch, public MicroNovaSwitchListener {
 public:
  MicroNovaSwitch(MicroNova *m) : MicroNovaSwitchListener(m) {}
  void dump_config() override {
    LOG_SWITCH("", "Micronova switch", this);
    this->dump_base_config();
  }

  void set_stove_state(bool v) override { this->publish_state(v); }
  bool get_stove_state() override { return this->state; }

  void set_memory_data_on(uint8_t f) { this->memory_data_on_ = f; }

  void set_memory_data_off(uint8_t f) { this->memory_data_off_ = f; }

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::micronova
