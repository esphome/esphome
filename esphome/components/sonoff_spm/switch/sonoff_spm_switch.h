#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../sonoff_spm.h"

namespace esphome {
namespace sonoff_spm {

class SonoffSPMSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void set_parent(SonoffSPM *parent) { this->parent_ = parent; }
  void set_relay_id(uint8_t relay_id) { this->relay_id_ = relay_id; }

  uint8_t get_relay_id() const { return this->relay_id_; }

 protected:
  void write_state(bool state) override;

  SonoffSPM *parent_{nullptr};
  uint8_t relay_id_{0};
};

}  // namespace sonoff_spm
}  // namespace esphome
