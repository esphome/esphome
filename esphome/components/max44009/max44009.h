#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace max44009 {

enum MAX44009Mode { MAX44009_MODE_AUTO, MAX44009_MODE_LOW_POWER, MAX44009_MODE_CONTINUOUS };

/// This class implements support for the MAX44009 Illuminance i2c sensor.
class MAX44009Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  MAX44009Sensor() {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void set_mode(MAX44009Mode mode);
  bool set_continuous_mode();
  bool set_low_power_mode();

 protected:
  /// Read the illuminance value
  float read_illuminance_();
  float convert_to_lux_(uint8_t data_high, uint8_t data_low);
  uint8_t read_(uint8_t reg);
  void write_(uint8_t reg, uint8_t value);

  int error_;
  MAX44009Mode mode_;
};

}  // namespace max44009
}  // namespace esphome
