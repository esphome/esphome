#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5angle8 {

class M5Angle8Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_sens_knob_position(int channel, sensor::Sensor *obj) { this->knob_pos_sensor_[channel] = obj; }
  
 protected:
    sensor::Sensor *knob_pos_sensor_[8];
    uint8_t fw_version_;
};

}  // namespace m5angle8
}  // namespace esphome
