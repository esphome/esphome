#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::bmp085 {

class BMP085Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)

 public:
  /// Schedule temperature+pressure readings.
  void update() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

 protected:
  struct CalibrationData {
    int16_t ac1, ac2, ac3;
    uint16_t ac4, ac5, ac6;
    int16_t b1, b2;
    int16_t mb, mc, md;
    float temp;
  };

  /// Internal method to read the temperature from the component after it has been scheduled.
  void read_temperature_();
  /// Internal method to read the pressure from the component after it has been scheduled.
  void read_pressure_();

  bool set_mode_(uint8_t mode);

  CalibrationData calibration_;
};

}  // namespace esphome::bmp085
