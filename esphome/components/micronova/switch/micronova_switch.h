#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome::micronova {

class MicroNovaSwitch : public switch_::Switch, public MicroNovaListener {
 public:
  MicroNovaSwitch(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override {
    LOG_SWITCH("", "Micronova switch", this);
    this->dump_base_config();
  }
  void request_value_from_stove() override {
    this->micronova_->request_address(this->memory_location_, this->memory_address_, this);
  }
  void process_value_from_stove(int value_from_stove) override;

  void set_memory_data_on(uint8_t f) { this->memory_data_on_ = f; }

  void set_memory_data_off(uint8_t f) { this->memory_data_off_ = f; }

 protected:
  void write_state(bool state) override;

  uint8_t memory_data_on_ = 0;
  uint8_t memory_data_off_ = 0;
  uint8_t raw_state_ = 0;
};

}  // namespace esphome::micronova
