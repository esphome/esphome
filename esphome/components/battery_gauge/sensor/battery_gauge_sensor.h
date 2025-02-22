#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::battery_gauge {

class BatteryGaugeSensor : public sensor::Sensor, public Component {
 public:
  BatteryGaugeSensor(sensor::Sensor *voltage_source, sensor::Sensor *current_source, float capacity, float efficiency,
                     float max_charge_voltage)
      : voltage_source_(voltage_source),
        current_source_(current_source),
        capacity_(capacity),
        efficiency_(efficiency),
        max_charge_voltage_(max_charge_voltage) {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_initial_state(float initial_state) { initial_state_ = initial_state; }

 protected:
  void publish_(float new_state);
  Sensor *voltage_source_;
  Sensor *current_source_;
  float capacity_;
  float efficiency_;
  void on_current_(float value);
  void on_voltage_(float value);
  float charge_state_{};           // charge state in Ah
  unsigned charge_percentage_{0};  // charge percentage * 10
  float max_charge_voltage_{0};
  float initial_state_{0};
  uint32_t last_time_{0};
  float filtered_voltage_{NAN};
  float filtered_current_{NAN};
  float last_current_{NAN};
  ESPPreferenceObject saved_percentage_;
};

}  // namespace esphome::battery_gauge
