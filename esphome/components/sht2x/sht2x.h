#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

#include <cinttypes>

namespace esphome {
namespace sht2x {

/// This class implements support for the SHT2x family of temperature+humidity i2c sensors.
class SHT2XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void set_heater_enabled(bool heater_enabled) { heater_enabled_ = heater_enabled; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  uint8_t crc8(const uint8_t *data, uint8_t len);
  uint16_t read_raw_value();
  float get_temperature();
  float get_humidity();
  uint8_t get_firmware_version();

  bool heater_enabled_{true};
};

}  // namespace sht2x
}  // namespace esphome
