#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include "../m5stack_8angle.h"

namespace esphome {
namespace m5stack_8angle {

class M5Stack8AngleKnobSensor : public sensor::Sensor,
                                public PollingComponent,
                                public Parented<M5Stack8AngleComponent> {
 public:
  void update() override;
  void set_channel(uint8_t channel) { this->channel_ = channel; };
  void set_bit_depth(AnalogBits bits) { this->bits_ = bits; };
  void set_raw(bool raw) { this->raw_ = raw; };

 protected:
  uint8_t channel_;
  AnalogBits bits_;
  bool raw_;
};

}  // namespace m5stack_8angle
}  // namespace esphome
