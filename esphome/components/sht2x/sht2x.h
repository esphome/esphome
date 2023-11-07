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
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  uint8_t crc8_(const uint8_t *data, uint8_t len);
  uint16_t read_raw_value_();
  void request_temperature_();
  void request_humidity_();
  void publish_temperature_();
  void publish_humidity_();
  void handle_temperature_();
  void handle_humidity_();
};

}  // namespace sht2x
}  // namespace esphome
