#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "switch/custom_switch.h"
#include "number/custom_number.h"
#include "opentherm.h"
#include "globals.h"
#include <queue>

namespace esphome {
namespace diyless_opentherm {

static const char *const TAG = "diyless_opentherm";

class DiyLessOpenThermComponent : public PollingComponent {
 public:
  bool ch_min_max_read = false;
  bool dhw_min_max_read = false;
  float confirmed_dhw_setpoint = 0;

  DiyLessOpenThermComponent() = default;

  void set_ch_min_temperature_sensor(sensor::Sensor *sensor) { ch_min_temperature_sensor_ = sensor; }
  void set_ch_max_temperature_sensor(sensor::Sensor *sensor) { ch_max_temperature_sensor_ = sensor; }
  void set_dhw_min_temperature_sensor(sensor::Sensor *sensor) { dhw_min_temperature_sensor_ = sensor; }
  void set_dhw_max_temperature_sensor(sensor::Sensor *sensor) { dhw_max_temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor_ = sensor; }
  void set_modulation_sensor(sensor::Sensor *sensor) { modulation_sensor_ = sensor; }
  void set_boiler_temperature_sensor(sensor::Sensor *sensor) { boiler_temperature_sensor_ = sensor; }
  void set_return_temperature_sensor(sensor::Sensor *sensor) { return_temperature_sensor_ = sensor; }
  void set_ch_active_binary_sensor(binary_sensor::BinarySensor *sensor) { ch_active_binary_sensor_ = sensor; }
  void set_dhw_active_binary_sensor(binary_sensor::BinarySensor *sensor) { dhw_active_binary_sensor_ = sensor; }
  void set_cooling_active_binary_sensor(binary_sensor::BinarySensor *sensor) { cooling_active_binary_sensor_ = sensor; }
  void set_flame_active_binary_sensor(binary_sensor::BinarySensor *sensor) { flame_active_binary_sensor_ = sensor; }
  void set_fault_binary_sensor(binary_sensor::BinarySensor *sensor) { fault_binary_sensor_ = sensor; }
  void set_diagnostic_binary_sensor(binary_sensor::BinarySensor *sensor) { diagnostic_binary_sensor_ = sensor; }
  void set_ch_enabled_switch(diyless_opentherm::CustomSwitch *switch_) { ch_enabled_switch_ = switch_; }
  void set_dhw_enabled_switch(diyless_opentherm::CustomSwitch *switch_) { dhw_enabled_switch_ = switch_; }
  void set_cooling_enabled_switch(diyless_opentherm::CustomSwitch *switch_) { cooling_enabled_switch_ = switch_; }
  void set_ch_setpoint_temperature_number(diyless_opentherm::CustomNumber *number) {
    ch_setpoint_temperature_number_ = number;
  }
  void set_dhw_setpoint_temperature_number(diyless_opentherm::CustomNumber *number) {
    dhw_setpoint_temperature_number_ = number;
  }

  void setup() override;
  void update() override;
  void loop() override;

  void initialize(char pin_in, char pin_out);
  void log_message(esp_log_level_t level, const char *pre_message, unsigned long message);
  void publish_sensor_state(sensor::Sensor *sensor, float state);
  void publish_binary_sensor_state(binary_sensor::BinarySensor *sensor, bool state);

  sensor::Sensor *ch_min_temperature_sensor_{nullptr};
  sensor::Sensor *ch_max_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_min_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_max_temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *modulation_sensor_{nullptr};
  sensor::Sensor *boiler_temperature_sensor_{nullptr};
  sensor::Sensor *return_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *ch_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *dhw_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *cooling_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *flame_active_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *fault_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *diagnostic_binary_sensor_{nullptr};
  diyless_opentherm::CustomSwitch *ch_enabled_switch_{nullptr};
  diyless_opentherm::CustomSwitch *dhw_enabled_switch_{nullptr};
  diyless_opentherm::CustomSwitch *cooling_enabled_switch_{nullptr};
  diyless_opentherm::CustomNumber *ch_setpoint_temperature_number_{nullptr};
  diyless_opentherm::CustomNumber *dhw_setpoint_temperature_number_{nullptr};

 private:
  std::queue<unsigned long> buffer_;
  unsigned long last_millis_ = 0;
  bool wanted_ch_enabled_ = false;
  bool wanted_dhw_enabled_ = false;
  bool wanted_cooling_enabled_ = false;

  void process_status_();
  void enqueue_request_(unsigned long request);
  const char *format_message_type_(unsigned long message);
};

extern DiyLessOpenThermComponent *component;

}  // namespace diyless_opentherm
}  // namespace esphome
