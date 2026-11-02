#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

#include <cinttypes>

namespace esphome::sht4x {

enum SHT4XPRECISION { SHT4X_PRECISION_HIGH = 0, SHT4X_PRECISION_MED, SHT4X_PRECISION_LOW };

enum SHT4XHEATERPOWER { SHT4X_HEATERPOWER_HIGH, SHT4X_HEATERPOWER_MED, SHT4X_HEATERPOWER_LOW };

enum SHT4XHEATERTIME : uint16_t { SHT4X_HEATERTIME_LONG = 1100, SHT4X_HEATERTIME_SHORT = 110 };

class SHT4XComponent final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(temp)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_precision_value(SHT4XPRECISION precision) { this->precision_ = precision; };
  void set_heater_power_value(SHT4XHEATERPOWER heater_power) { this->heater_power_ = heater_power; };
  void set_heater_time_value(SHT4XHEATERTIME heater_time) { this->heater_time_ = heater_time; };
  void set_heater_duty_value(float duty_cycle) { this->duty_cycle_ = duty_cycle; };

 protected:
  SHT4XPRECISION precision_;
  SHT4XHEATERPOWER heater_power_;
  SHT4XHEATERTIME heater_time_;
  float duty_cycle_;

  void read_serial_number_();
  uint8_t heater_command_;
  uint32_t heater_interval_{0};
  uint32_t last_heater_millis_{0};
  uint32_t serial_number_;
};

}  // namespace esphome::sht4x
