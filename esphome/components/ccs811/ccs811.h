#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ccs811 {

class CCS811Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(co2)
  SUB_SENSOR(tvoc)
  SUB_TEXT_SENSOR(version)

 public:
  void set_baseline(uint16_t baseline) { baseline_ = baseline; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }

  /// Setup the sensor and test for a connection.
  void setup() override;
  /// Schedule temperature+pressure readings.
  void update() override;

  void dump_config() override;

 protected:
  optional<uint8_t> read_status_() { return this->read_byte(0x00); }
  bool status_has_error_() { return this->read_status_().value_or(1) & 1; }
  bool status_app_is_valid_() { return this->read_status_().value_or(0) & (1 << 4); }
  bool status_has_data_() { return this->read_status_().value_or(0) & (1 << 3); }
  void send_env_data_();

  optional<uint16_t> baseline_{};
  /// Input sensor for humidity reading.
  sensor::Sensor *humidity_{nullptr};
  /// Input sensor for temperature reading.
  sensor::Sensor *temperature_{nullptr};
};

}  // namespace esphome::ccs811
