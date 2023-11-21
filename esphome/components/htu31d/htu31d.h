#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace htu31d {

class HTU31DComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;        /// Setup (reset) the sensor and check connection.
  void update() override;       /// Update the sensor values (temperature+humidity).
  void dump_config() override;  /// Dumps the configuration values.

  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }

  void set_heater_state(bool desired);
  bool is_heater_enabled();

  float get_setup_priority() const override;

 private:
  bool reset_();
  uint32_t read_serial_num_();
  uint8_t compute_crc_(uint32_t value);

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
};
}  // namespace htu31d
}  // namespace esphome
