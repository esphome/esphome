#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::cw2017 {

static constexpr uint8_t PROFILE_SIZE = 80;

class CW2017 : public PollingComponent, public i2c::I2CDevice {
 public:
  explicit CW2017(const uint8_t *profile) : profile_(profile) {}

  void set_battery_level_sensor(sensor::Sensor *sensor) { this->battery_level_sensor_ = sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { this->battery_voltage_sensor_ = sensor; }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void init_();
  void init_failed_(const LogString *reason);
  bool read_profile_matches_(bool &matches);
  bool upload_profile_();
  void restart_();
  void restart_step_(uint8_t step);
  void wait_ready_(uint8_t attempt);

  const uint8_t *profile_;
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  bool ready_{false};
  bool restart_pending_{false};
  uint8_t init_attempts_{0};
};

}  // namespace esphome::cw2017
