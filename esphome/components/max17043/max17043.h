#pragma once

#include "esphome/core/component.h"
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

  void set_model(MAX17043Model model) { model_ = model; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_battery_remaining_sensor(sensor::Sensor *battery_remaining_sensor) {
    battery_remaining_sensor_ = battery_remaining_sensor;
  }
  void set_charge_rate_sensor(sensor::Sensor *charge_rate_sensor) { charge_rate_sensor_ = charge_rate_sensor; }

 protected:
  bool is_max17048_() const { return this->model_ == MAX17043Model::MAX17043_MODEL_MAX17048; }
  const char *model_name_() const { return this->is_max17048_() ? "MAX17048" : "MAX17043"; }

  /// Identify the gauge and clear any stale sleep bit. Returns false if it did not answer,
  /// which is retryable; an answer from the wrong device marks the component failed instead.
  bool configure_();

  MAX17043Model model_{MAX17043Model::MAX17043_MODEL_MAX17043};
  bool configured_{false};
  uint8_t configure_attempts_{0};
  uint16_t version_{0};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
  sensor::Sensor *charge_rate_sensor_{nullptr};
};

}  // namespace esphome::max17043
