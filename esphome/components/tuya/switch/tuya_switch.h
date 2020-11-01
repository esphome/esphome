#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace tuya {

class TuyaSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  Tuya *parent_;
  uint8_t switch_id_{0};
};

}  // namespace tuya
}  // namespace esphome
