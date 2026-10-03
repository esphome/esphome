#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/optional.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

#include <cstdint>

namespace esphome::tmp102 {

enum TMP102ConversionRate : uint8_t {
  TMP102_CONVERSION_RATE_0_25HZ = 0b00,
  TMP102_CONVERSION_RATE_1HZ = 0b01,
  TMP102_CONVERSION_RATE_4HZ = 0b10,
  TMP102_CONVERSION_RATE_8HZ = 0b11,
};

enum TMP102ThermostatMode : uint8_t {
  TMP102_THERMOSTAT_MODE_COMPARATOR = 0,
  TMP102_THERMOSTAT_MODE_INTERRUPT = 1,
};

enum TMP102AlertPolarity : uint8_t {
  TMP102_ALERT_POLARITY_ACTIVE_LOW = 0,
  TMP102_ALERT_POLARITY_ACTIVE_HIGH = 1,
};

enum TMP102LimitType : uint8_t {
  TMP102_LIMIT_LOW = 0,
  TMP102_LIMIT_HIGH = 1,
};

class TMP102Component final : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  void setup() override;
  void set_configure(bool configure) { this->configure_ = configure; }
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_extended_mode(bool extended_mode) { this->extended_mode_ = extended_mode; }
  void set_conversion_rate(TMP102ConversionRate rate) { this->conversion_rate_ = rate; }
  void set_one_shot_mode(bool one_shot_mode) { this->one_shot_mode_ = one_shot_mode; }
  void set_alert_polarity(TMP102AlertPolarity pol) { this->alert_polarity_ = pol; }
  void set_thermostat_mode(TMP102ThermostatMode mode) { this->thermostat_mode_ = mode; }
  void set_fault_queue(uint8_t faults) { this->fault_queue_ = faults; }
  void set_temperature_high(float temp) { this->temperature_high_ = temp; }
  void set_temperature_low(float temp) { this->temperature_low_ = temp; }

  float get_limit_temperature(TMP102LimitType limit) const;

 protected:
  bool write_config_register_();
  bool write_limit_register_(uint8_t reg, float temperature);
  bool check_configuration_();
  void read_failed_();
  void read_temperature_();

  bool extended_mode_{false};
  TMP102ConversionRate conversion_rate_{TMP102_CONVERSION_RATE_4HZ};
  bool one_shot_mode_{false};
  TMP102AlertPolarity alert_polarity_{TMP102_ALERT_POLARITY_ACTIVE_LOW};
  TMP102ThermostatMode thermostat_mode_{TMP102_THERMOSTAT_MODE_COMPARATOR};
  uint8_t fault_queue_{1};

  // Cached from write_config_register_() so update() can set OS=1 without recomputing
  uint8_t config_high_byte_{0x00};
  uint8_t config_low_byte_{0x80};  // CR1:CR0 = 10 (4 Hz default)

  optional<float> temperature_high_;
  optional<float> temperature_low_;
  bool configure_{false};
  bool setup_complete_{false};
  bool conversion_pending_{false};
};

}  // namespace esphome::tmp102
