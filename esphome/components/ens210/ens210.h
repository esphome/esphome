#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ens210 {

/// This class implements support for the ENS210 relative humidity and temperature i2c sensor.
class ENS210Component : public PollingComponent, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override;
  void dump_config() override;
  void setup() override;
  void update() override;

  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

  enum ErrorCode {
    ENS210_STATUS_OK = 0,     // The value was read, the CRC matches, and data is valid
    ENS210_STATUS_INVALID,    // The value was read, the CRC matches, but the data is invalid (e.g. the measurement was
                              // not yet finished)
    ENS210_STATUS_CRC_ERROR,  // The value was read, but the CRC over the payload (valid and data) does not match
    ENS210_STATUS_I2C_ERROR,  // There was an I2C communication error
    ENS210_WRONG_CHIP_ID      // The read PART_ID is not the expected part id of the ENS210
  } error_code_{ENS210_STATUS_OK};

 protected:
  bool set_low_power_(bool enable);
  void extract_measurement_(uint32_t val, int *data, int *status);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace ens210
}  // namespace esphome
