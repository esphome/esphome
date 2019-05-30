#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace htu21d {

class HTU21DComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }

  /// Setup (reset) the sensor and check connection.
  void setup() override;
  void dump_config() override;
  /// Update the sensor values (temperature+humidity).
  void update() override;

  float get_setup_priority() const override;

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
};

}  // namespace htu21d
}  // namespace esphome
