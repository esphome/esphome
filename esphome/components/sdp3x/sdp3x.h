#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sdp3x {

class SDP3XComponent : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  /// Schedule temperature+pressure readings.
  void update() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  /// Internal method to read the pressure from the component after it has been scheduled.
  void read_pressure_();

  bool check_crc_(const uint8_t data[], uint8_t size, uint8_t checksum);

  float pressure_scale_factor_ = 0.0f; // hPa per count
};

}  // namespace sdp3x
}  // namespace esphome
