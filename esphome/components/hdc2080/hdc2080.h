#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace hdc2080 {

class HDC2080Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { this->temperature_sensor_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_sensor_ = humidity; }

  /// Setup the sensor and check for connection.
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void read_temperature_();
  void read_humidity_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  enum State : uint8_t {
    IDLE,
    READ_TEMPERATURE,
    READ_HUMIDITY,
  } state_{IDLE};
};

}  // namespace hdc2080
}  // namespace esphome
