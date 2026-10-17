#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::max17043 {

enum class MAX17043Model : uint8_t {
  MAX17043_MODEL_MAX17043,
  MAX17043_MODEL_MAX17048,
};

class MAX17043Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void sleep_mode();

  void set_model(MAX17043Model model) { this->model_ = model; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }
  void set_battery_remaining_sensor(sensor::Sensor *battery_remaining_sensor) {
    this->battery_remaining_sensor_ = battery_remaining_sensor;
  }
#ifdef USE_MAX17048
  void set_charge_rate_sensor(sensor::Sensor *charge_rate_sensor) { this->charge_rate_sensor_ = charge_rate_sensor; }
#endif

 protected:
  bool is_max17048_() const { return this->model_ == MAX17043Model::MAX17043_MODEL_MAX17048; }
  const LogString *model_name_() const { return this->is_max17048_() ? LOG_STR("MAX17048") : LOG_STR("MAX17043"); }

  /// Verify and wake the gauge; false if it did not answer.
  bool configure_();

  MAX17043Model model_{MAX17043Model::MAX17043_MODEL_MAX17043};
  bool configured_{false};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
#ifdef USE_MAX17048
  sensor::Sensor *charge_rate_sensor_{nullptr};
#endif
};

}  // namespace esphome::max17043
