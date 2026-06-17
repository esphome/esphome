#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

#include "../m5stack_8angle.h"

namespace esphome::m5stack_8angle {

class M5Stack8AngleSwitchBinarySensor : public binary_sensor::BinarySensor,
                                        public PollingComponent,
                                        public Parented<M5Stack8AngleComponent> {
 public:
  void update() override;
};

}  // namespace esphome::m5stack_8angle
