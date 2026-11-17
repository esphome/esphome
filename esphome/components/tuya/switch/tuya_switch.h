#pragma once

#include "esphome/components/tuya/tuya.h"
#include "esphome/components/switch/switch.h"

namespace esphome::tuya {

class TuyaSwitch final : public switch_::Switch {
 public:
  // Called from generated code before setup, after the datapoint setters
  void register_listeners();
  // Called from the hub's dump_config()
  void dump_child_config();
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  Tuya *parent_;
  uint8_t switch_id_{0};
};

}  // namespace esphome::tuya
