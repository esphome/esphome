#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::xdb401 {

class XDB401Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }
  void set_pressure_range_bar(uint8_t pressure_range_bar) { this->pressure_range_bar_ = pressure_range_bar; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void handle_comm_failure_(const char *message);
  i2c::ErrorCode start_measurement_();
  void check_measurement_ready_(uint8_t attempt);
  void read_measurement_();
  i2c::ErrorCode read_pressure_(float &pressure);
  i2c::ErrorCode read_temperature_(float &temperature);

  static constexpr float full_scale_pressure_pa(uint8_t pressure_range_bar) { return pressure_range_bar * 100000.0f; }

  uint8_t comm_err_counter_{0};
  bool measurement_in_progress_{false};
  uint8_t pressure_range_bar_{10};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace esphome::xdb401
