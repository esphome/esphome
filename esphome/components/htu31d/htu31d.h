#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::htu31d {

class HTU31DComponent final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;        /// Setup (reset) the sensor and check connection.
  void update() override;       /// Update the sensor values (temperature+humidity).
  void dump_config() override;  /// Dumps the configuration values.

  void set_heater_state(bool desired);
  bool is_heater_enabled();

 protected:
  bool reset_();
  uint32_t read_serial_num_();
};
}  // namespace esphome::htu31d
