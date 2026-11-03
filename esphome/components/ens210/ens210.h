#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ens210 {

/// This class implements support for the ENS210 relative humidity and temperature i2c sensor.
class ENS210Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(humidity)
  SUB_SENSOR(temperature)

 public:
  void dump_config() override;
  void setup() override;
  void update() override;

  enum ErrorCode {
    ENS210_STATUS_OK = 0,     // The value was read, the CRC matches, and data is valid
    ENS210_STATUS_INVALID,    // The value was read, the CRC matches, but the data is invalid (e.g. the measurement was
                              // not yet finished)
    ENS210_STATUS_CRC_ERROR,  // The value was read, but the CRC over the payload (valid and data) does not match
    ENS210_STATUS_I2C_ERROR,  // There was an I2C communication error
  };

 protected:
  bool set_low_power_(bool enable);
  void extract_measurement_(uint32_t val, int *data, int *status);
};

}  // namespace esphome::ens210
