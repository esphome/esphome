#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/tinovi/i2cArduino/blob/master/i2cArduino.h

namespace esphome {
namespace pmwcs3 {

class PMWCS3Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_e25_sensor(sensor::Sensor *e25_sensor) { e25_sensor_ = e25_sensor; }
  void set_ec_sensor(sensor::Sensor *ec_sensor) { ec_sensor_ = ec_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_vwc_sensor(sensor::Sensor *vwc_sensor) { vwc_sensor_ = vwc_sensor; }

  void change_i2c_address(uint8_t newaddress);
  void set_air_calibration();
  void set_water_calibration();

 protected:
  void read_data_();

  sensor::Sensor *e25_sensor_{nullptr};
  sensor::Sensor *ec_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *vwc_sensor_{nullptr};
};

}  // namespace pmwcs3
}  // namespace esphome
