#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace hdc302x {

enum HDC302XPowerMode : uint8_t {
  HIGH_ACCURACY = 0x00,
  BALANCED = 0x0b,
  LOW_POWER = 0x16,
  ULTRA_LOW_POWER = 0xff,
};

/**
 HDC302x Temperature and humidity sensor.

 Datasheet:
 https://www.ti.com/lit/ds/symlink/hdc3020.pdf
 */
class HDC302XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  bool enable_heater();
  bool disable_heater();

  void set_temp_sensor(sensor::Sensor *temp_sensor) { this->temp_sensor_ = temp_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

  void set_power_mode(HDC302XPowerMode power_mode) { this->power_mode_ = power_mode; }
  void set_heater_enabled(bool heater_enabled) { this->heater_enabled_ = heater_enabled; }

 protected:
  sensor::Sensor *temp_sensor_;
  sensor::Sensor *humidity_sensor_;

  HDC302XPowerMode power_mode_{HDC302XPowerMode::HIGH_ACCURACY};
  bool heater_enabled_{false};

  uint8_t crc8_(const uint8_t *buf, size_t len);
  void read_data_();
  uint32_t conversion_delay_ms_();
};

}  // namespace hdc302x
}  // namespace esphome
