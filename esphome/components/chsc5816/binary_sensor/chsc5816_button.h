#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/chsc5816/touchscreen/chsc5816_touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace chsc5816 {

class CHSC5816Button : public binary_sensor::BinarySensor,
                       public Component,
                       public CHSC5816ButtonListener,
                       public Parented<CHSC5816Touchscreen> {
 public:
  void setup() override {
    this->parent_->register_button_listener(this);
    this->publish_initial_state(false);
  }

  void dump_config() override { LOG_BINARY_SENSOR("", "CHSC5816 Button", this); }

  void update_button(bool state) override { this->publish_state(state); }
};

}  // namespace chsc5816
}  // namespace esphome
