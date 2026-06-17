#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../touchscreen/cst328_touchscreen.h"

namespace esphome::cst328 {

class CST328Button : public binary_sensor::BinarySensor,
                     public Component,
                     public CST328ButtonListener,
                     public Parented<CST328Touchscreen> {
 public:
  void setup() override;
  void dump_config() override;
  void update_button(bool state) override;
};

}  // namespace esphome::cst328
