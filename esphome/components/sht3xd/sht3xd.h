#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sht3xd {

/// This class implements support for the SHT3x-DIS family of temperature+humidity i2c sensors.
class SHT3XDComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  bool write_command_(uint16_t command);
  bool read_data_(uint16_t *data, uint8_t len);

  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *humidity_sensor_;
};

}  // namespace sht3xd
}  // namespace esphome
