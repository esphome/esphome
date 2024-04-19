#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mpl115a2 {

// enums from https://github.com/adafruit/Adafruit_MPL115A2/
/** MPL115A2 registers **/
enum {
  MPL115A2_REGISTER_PRESSURE_MSB = (0x00),
  MPL115A2_REGISTER_PRESSURE_LSB = (0x01),

  MPL115A2_REGISTER_TEMP_MSB = (0x02),
  MPL115A2_REGISTER_TEMP_LSB = (0x03),

  MPL115A2_REGISTER_A0_COEFF_MSB = (0x04),

  MPL115A2_REGISTER_STARTCONVERSION = (0x12),
};

class MPL115A2Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_pressure(sensor::Sensor *pressure) { pressure_ = pressure; }

  void setup() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 private:
  float mpl115a2_a0_;
  float mpl115a2_b1_;
  float mpl115a2_b2_;
  float mpl115a2_c12_;

  void read_coefficients_();

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *pressure_{nullptr};
};

}  // namespace mpl115a2
}  // namespace esphome
