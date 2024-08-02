#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

#include "../m5stack_8angle.h"

namespace esphome {
namespace m5stack_8angle {

class M5Stack8AngleSwitchBinarySensor : public binary_sensor::BinarySensor,
                                        public PollingComponent,
                                        public Parented<M5Stack8AngleComponent> {
 public:
  void update() override;
};

}  // namespace m5stack_8angle
}  // namespace esphome
