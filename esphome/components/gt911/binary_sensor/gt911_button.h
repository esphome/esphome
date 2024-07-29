#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/gt911/touchscreen/gt911_touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace gt911 {

class GT911Button : public binary_sensor::BinarySensor,
                    public Component,
                    public GT911ButtonListener,
                    public Parented<GT911Touchscreen> {
 public:
  void setup() override;
  void dump_config() override;

  void set_index(uint8_t index) { this->index_ = index; }

  void update_button(uint8_t index, bool state) override;

 protected:
  uint8_t index_;
};

}  // namespace gt911
}  // namespace esphome
