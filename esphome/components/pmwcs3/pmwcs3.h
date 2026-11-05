#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/tinovi/i2cArduino/blob/master/i2cArduino.h

namespace esphome::pmwcs3 {

class PMWCS3Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(e25)
  SUB_SENSOR(ec)
  SUB_SENSOR(temperature)
  SUB_SENSOR(vwc)

 public:
  void update() override;
  void dump_config() override;

  void new_i2c_address(uint8_t newaddress);
  void air_calibration();
  void water_calibration();

 protected:
  void read_data_();
};

}  // namespace esphome::pmwcs3
