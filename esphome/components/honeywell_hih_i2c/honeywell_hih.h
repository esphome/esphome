// Honeywell HumidIcon I2C Sensors
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::honeywell_hih_i2c {

class HoneywellHIComponent final : public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void loop() override;
  void update() override;

 protected:
  bool measurement_running_{false};
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
 private:
  void read_sensor_data_();
  void start_measurement_();
  bool is_measurement_ready_();
  void measurement_timeout_();
};

}  // namespace esphome::honeywell_hih_i2c
