#pragma once

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::aranet {

class Aranet final : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  explicit Aranet(uint64_t address) : address_(address) {}

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *sensor) { this->co2_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { this->pressure_sensor_ = sensor; }
  void set_battery_level_sensor(sensor::Sensor *sensor) { this->battery_level_sensor_ = sensor; }
  void set_measurement_interval_sensor(sensor::Sensor *sensor) { this->measurement_interval_sensor_ = sensor; }
  void set_measurement_age_sensor(sensor::Sensor *sensor) { this->measurement_age_sensor_ = sensor; }
  void set_signal_strength_sensor(sensor::Sensor *sensor) { this->signal_strength_sensor_ = sensor; }
  void set_radon_sensor(sensor::Sensor *sensor) { this->radon_sensor_ = sensor; }
  void set_radiation_rate_sensor(sensor::Sensor *sensor) { this->radiation_rate_sensor_ = sensor; }
  void set_radiation_total_sensor(sensor::Sensor *sensor) { this->radiation_total_sensor_ = sensor; }
  void set_radiation_duration_sensor(sensor::Sensor *sensor) { this->radiation_duration_sensor_ = sensor; }

 protected:
  uint64_t address_;
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *measurement_interval_sensor_{nullptr};
  sensor::Sensor *measurement_age_sensor_{nullptr};
  sensor::Sensor *signal_strength_sensor_{nullptr};
  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radiation_rate_sensor_{nullptr};
  sensor::Sensor *radiation_total_sensor_{nullptr};
  sensor::Sensor *radiation_duration_sensor_{nullptr};
  uint8_t last_measurement_counter_{0};
  bool has_measurement_{false};
  bool has_warned_invalid_length_{false};
  bool has_warned_integration_disabled_{false};
  bool has_warned_unsupported_type_{false};
};

}  // namespace esphome::aranet
