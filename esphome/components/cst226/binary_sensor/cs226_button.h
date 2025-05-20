#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../touchscreen/cst226_touchscreen.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace cst226 {

class CST226Button : public binary_sensor::BinarySensor,
                     public Component,
                     public CST226ButtonListener,
                     public Parented<CST226Touchscreen> {
 public:
  void setup() override;
  void dump_config() override;

  void update_button(bool state) override;
};

}  // namespace cst226
}  // namespace esphome
