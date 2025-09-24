#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace htu31d {

class HTU31DComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;        /// Setup (reset) the sensor and check connection.
  void update() override;       /// Update the sensor values (temperature+humidity).
  void dump_config() override;  /// Dumps the configuration values.

  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_ = humidity; }

  void set_heater_state(bool desired);
  bool is_heater_enabled();

  float get_setup_priority() const override;

 protected:
  bool reset_();
  uint32_t read_serial_num_();

  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
};
}  // namespace htu31d
}  // namespace esphome
