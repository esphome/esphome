#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome {
namespace shtcx {

enum SHTCXType { SHTCX_TYPE_SHTC3 = 0, SHTCX_TYPE_SHTC1, SHTCX_TYPE_UNKNOWN };

/// This class implements support for the SHT3x-DIS family of temperature+humidity i2c sensors.
class SHTCXComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void soft_reset();
  void sleep();
  void wake_up();

 protected:
  SHTCXType type_;
  uint16_t sensor_id_;
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace shtcx
}  // namespace esphome
