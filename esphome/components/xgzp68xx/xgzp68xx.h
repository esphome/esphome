#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xgzp68xx {

class XGZP68XXComponent : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  void set_k_value(uint16_t k_value) { this->k_value_ = k_value; }

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  /// Internal method to read the pressure from the component after it has been scheduled.
  void read_pressure_();
  uint16_t k_value_;
};

}  // namespace xgzp68xx
}  // namespace esphome
