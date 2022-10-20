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
  bool CHMinMaxRead = false;
  bool DHWMinMaxRead = false;
  float confirmedDHWSetpoint = 0;

  DiyLessOpenThermComponent() = default;

  void set_ch_min_temperature_sensor(sensor::Sensor *sensor) { ch_min_temperature_ = sensor; }
  void set_ch_max_temperature_sensor(sensor::Sensor *sensor) { ch_max_temperature_ = sensor; }
  void set_dhw_min_temperature_sensor(sensor::Sensor *sensor) { dhw_min_temperature_ = sensor; }
  void set_dhw_max_temperature_sensor(sensor::Sensor *sensor) { dhw_max_temperature_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { pressure_ = sensor; }
  void set_modulation_sensor(sensor::Sensor *sensor) { modulation_ = sensor; }
  void set_boiler_temperature_sensor(sensor::Sensor *sensor) { boiler_temperature_ = sensor; }
  void set_return_temperature_sensor(sensor::Sensor *sensor) { return_temperature_ = sensor; }
  void set_ch_active_binary_sensor(binary_sensor::BinarySensor *sensor) { ch_active_ = sensor; }
  void set_dhw_active_binary_sensor(binary_sensor::BinarySensor *sensor) { dhw_active_ = sensor; }
  void set_cooling_active_binary_sensor(binary_sensor::BinarySensor *sensor) { cooling_active_ = sensor; }
  void set_flame_active_binary_sensor(binary_sensor::BinarySensor *sensor) { flame_active_ = sensor; }
  void set_fault_binary_sensor(binary_sensor::BinarySensor *sensor) { fault_ = sensor; }
  void set_diagnostic_binary_sensor(binary_sensor::BinarySensor *sensor) { diagnostic_ = sensor; }
  void set_ch_enabled_switch(diyless_opentherm::CustomSwitch *switch_) { ch_enabled_ = switch_; }
  void set_dhw_enabled_switch(diyless_opentherm::CustomSwitch *switch_) { dhw_enabled_ = switch_; }
  void set_cooling_enabled_switch(diyless_opentherm::CustomSwitch *switch_) { cooling_enabled_ = switch_; }
  void set_ch_setpoint_temperature_number(diyless_opentherm::CustomNumber *number) { ch_setpoint_temperature_ = number; }
  void set_dhw_setpoint_temperature_number(diyless_opentherm::CustomNumber *number) { dhw_setpoint_temperature_ = number; }

  void setup() override;
  void update() override;
  void loop() override;

  void initialize(char pinIn, char pinOut);
  void logMessage(esp_log_level_t level, const char* preMessage, unsigned long message);
  void publish_sensor_state(sensor::Sensor *sensor, float state);
  void publish_binary_sensor_state(binary_sensor::BinarySensor *sensor, bool state);

  sensor::Sensor *ch_min_temperature_{nullptr};
  sensor::Sensor *ch_max_temperature_{nullptr};
  sensor::Sensor *dhw_min_temperature_{nullptr};
  sensor::Sensor *dhw_max_temperature_{nullptr};
  sensor::Sensor *pressure_{nullptr};
  sensor::Sensor *modulation_{nullptr};
  sensor::Sensor *boiler_temperature_{nullptr};
  sensor::Sensor *return_temperature_{nullptr};
  binary_sensor::BinarySensor *ch_active_{nullptr};
  binary_sensor::BinarySensor *dhw_active_{nullptr};
  binary_sensor::BinarySensor *cooling_active_{nullptr};
  binary_sensor::BinarySensor *flame_active_{nullptr};
  binary_sensor::BinarySensor *fault_{nullptr};
  binary_sensor::BinarySensor *diagnostic_{nullptr};
  diyless_opentherm::CustomSwitch *ch_enabled_{nullptr};
  diyless_opentherm::CustomSwitch *dhw_enabled_{nullptr};
  diyless_opentherm::CustomSwitch *cooling_enabled_{nullptr};
  diyless_opentherm::CustomNumber *ch_setpoint_temperature_{nullptr};
  diyless_opentherm::CustomNumber *dhw_setpoint_temperature_{nullptr};

 private:
  std::queue<unsigned long> buffer;
  unsigned long lastMillis = 0;
  bool CHEnabled = 0;
  bool DHWEnabled = 0;
  bool coolingEnabled = 0;

  void processStatus();
  void enqueueRequest(unsigned long request);
  const char* formatMessageType(unsigned long message);
};

extern DiyLessOpenThermComponent *component;

} // namespace diyless_opentherm
} // namespace esphome
