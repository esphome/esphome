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

/* HDC302x heater power configs, per datasheet Table 7-15. */
static const uint16_t HDC302X_HEATER_POWER_FULL = 0x3fff;
static const uint16_t HDC302X_HEATER_POWER_HALF = 0x03ff;
static const uint16_t HDC302X_HEATER_POWER_QUARTER = 0x009f;

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
  bool configure_heater(uint16_t power_level);
  bool disable_heater();

  void set_temp_sensor(sensor::Sensor *temp_sensor) { this->temp_sensor_ = temp_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

  void set_power_mode(HDC302XPowerMode power_mode) { this->power_mode_ = power_mode; }

 protected:
  sensor::Sensor *temp_sensor_;
  sensor::Sensor *humidity_sensor_;

  HDC302XPowerMode power_mode_{HDC302XPowerMode::HIGH_ACCURACY};

  void read_data_();
  uint32_t conversion_delay_ms_();
};

}  // namespace hdc302x
}  // namespace esphome
