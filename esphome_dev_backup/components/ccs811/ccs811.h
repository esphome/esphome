#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ccs811 {

class CCS811Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_co2(sensor::Sensor *co2) { co2_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_ = tvoc; }
  void set_version(text_sensor::TextSensor *version) { version_ = version; }
  void set_baseline(uint16_t baseline) { baseline_ = baseline; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }

  /// Setup the sensor and test for a connection.
  void setup() override;
  /// Schedule temperature+pressure readings.
  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  optional<uint8_t> read_status_() { return this->read_byte(0x00); }
  bool status_has_error_() { return this->read_status_().value_or(1) & 1; }
  bool status_app_is_valid_() { return this->read_status_().value_or(0) & (1 << 4); }
  bool status_has_data_() { return this->read_status_().value_or(0) & (1 << 3); }
  void send_env_data_();

  enum ErrorCode {
    UNKNOWN,
    COMMUNICATION_FAILED,
    INVALID_ID,
    SENSOR_REPORTED_ERROR,
    APP_INVALID,
    APP_START_FAILED,
  } error_code_{UNKNOWN};

  sensor::Sensor *co2_{nullptr};
  sensor::Sensor *tvoc_{nullptr};
  text_sensor::TextSensor *version_{nullptr};
  optional<uint16_t> baseline_{};
  /// Input sensor for humidity reading.
  sensor::Sensor *humidity_{nullptr};
  /// Input sensor for temperature reading.
  sensor::Sensor *temperature_{nullptr};
};

}  // namespace ccs811
}  // namespace esphome
