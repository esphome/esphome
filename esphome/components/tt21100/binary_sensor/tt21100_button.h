#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/tt21100/touchscreen/tt21100.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tt21100 {

class TT21100Button : public binary_sensor::BinarySensor,
                      public Component,
                      public TT21100ButtonListener,
                      public Parented<TT21100Touchscreen> {
 public:
  void setup() override;
  void dump_config() override;

  void set_index(uint8_t index) { this->index_ = index; }

  void update_button(uint8_t index, uint16_t state) override;

 protected:
  uint8_t index_;
};

}  // namespace tt21100
}  // namespace esphome
