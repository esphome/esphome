#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome::micronova {

class MicroNovaButton : public Component, public button::Button, public MicroNovaBaseListener {
 public:
  MicroNovaButton(MicroNova *m) : MicroNovaBaseListener(m) {}
  void dump_config() override;

  void set_memory_data(uint8_t f) { this->memory_data_ = f; }
  uint8_t get_memory_data() { return this->memory_data_; }

 protected:
  void press_action() override;

  uint8_t memory_data_ = 0;
};

}  // namespace esphome::micronova
