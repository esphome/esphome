#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/cst816/touchscreen/cst816_touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace cst816 {

class CST816Button : public binary_sensor::BinarySensor,
                     public Component,
                     public CST816ButtonListener,
                     public Parented<CST816Touchscreen> {
 public:
  void setup() override {
    this->parent_->register_button_listener(this);
    this->publish_initial_state(false);
  }

  void dump_config() override { LOG_BINARY_SENSOR("", "CST816 Button", this); }

  void update_button(bool state) override { this->publish_state(state); }
};

}  // namespace cst816
}  // namespace esphome
