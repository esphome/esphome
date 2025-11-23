#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pm2005 {

enum SensorType {
  PM2005,
  PM2105,
};

class PM2005Component : public PollingComponent, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void set_sensor_type(SensorType sensor_type) { this->sensor_type_ = sensor_type; }

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor) { this->pm_1_0_sensor_ = pm_1_0_sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { this->pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { this->pm_10_0_sensor_ = pm_10_0_sensor; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  uint8_t sensor_situation_{0};
  uint8_t data_buffer_[12];
  SensorType sensor_type_{PM2005};

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  uint8_t situation_value_index_{3};
  uint8_t pm_1_0_value_index_{4};
  uint8_t pm_2_5_value_index_{6};
  uint8_t pm_10_0_value_index_{8};
  uint8_t measuring_value_index_{10};
};

}  // namespace pm2005
}  // namespace esphome
