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
  float _mpl115a2_a0;
  float _mpl115a2_b1;
  float _mpl115a2_b2;
  float _mpl115a2_c12;

  void readCoefficients();

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *pressure_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};
};

}  // namespace mpl115a2
}  // namespace esphome
