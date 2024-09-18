#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace hdc302x {

class HDC302xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }

  /// Setup the sensor and check for connection.
  void setup() override;
  void dump_config() override;
  /// Retrieve the latest sensor values.
  void update() override;

  float get_setup_priority() const override;

  template<size_t N> inline i2c::ErrorCode safe_write(const uint8_t (&data)[N]);
  template<size_t N> inline i2c::ErrorCode safe_read(uint8_t (&data)[N]);

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
};

}  // namespace hdc302x
}  // namespace esphome
