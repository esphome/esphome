#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace micronova {

class MicroNovaButton : public Component, public button::Button, public MicroNovaButtonListener {
 public:
  MicroNovaButton(MicroNova *m) : MicroNovaButtonListener(m) {}
  void dump_config() override { LOG_BUTTON("", "Micronova button", this); }

  void set_memory_data(uint8_t f) { this->memory_data_ = f; }
  uint8_t get_memory_data() { return this->memory_data_; }

 protected:
  void press_action() override;
};

}  // namespace micronova
}  // namespace esphome
