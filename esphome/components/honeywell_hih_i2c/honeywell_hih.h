// Honeywell HumidIcon I2C Sensors
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace honeywell_hih_i2c {

class HoneywellHIComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;
  void update() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

 protected:
  bool measurement_running_{false};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

 private:
  void read_sensor_data_();
  void start_measurement_();
  bool is_measurement_ready_();
  void measurement_timeout_();
};

}  // namespace honeywell_hih_i2c
}  // namespace esphome
