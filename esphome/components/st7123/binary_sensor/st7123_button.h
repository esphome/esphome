#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/st7123/touchscreen/st7123_touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace st7123 {

class ST7123Button : public binary_sensor::BinarySensor,
                     public Component,
                     public ST7123ButtonListener,
                     public Parented<ST7123Touchscreen> {
 public:
  void setup() override;
  void dump_config() override;

  void set_index(uint8_t index) { this->index_ = index; }

  void update_button(uint8_t index, bool state) override;

 protected:
  uint8_t index_;
};

}  // namespace st7123
}  // namespace esphome
