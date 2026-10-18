#pragma once

#include <cinttypes>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::duty_time_sensor {

class DutyTimeSensor final : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void start();
  void stop();
  bool is_running() const { return this->last_state_; }
  void reset() { this->set_value_(0); }

#ifdef USE_BINARY_SENSOR
  void set_sensor(binary_sensor::BinarySensor *sensor);
#endif
  void set_lambda(std::function<bool()> &&func) { this->func_ = func; }
  void set_last_duty_time_sensor(sensor::Sensor *sensor) { this->last_duty_time_sensor_ = sensor; }
  void set_restore(bool restore) { this->restore_ = restore; }

 protected:
  void set_value_(uint32_t sec);
  void process_state_(bool state);
  void publish_and_save_(uint32_t sec, uint32_t ms);

  std::function<bool()> func_{nullptr};
  sensor::Sensor *last_duty_time_sensor_{nullptr};
  ESPPreferenceObject pref_;

  uint32_t total_sec_{0};
  uint32_t last_time_{0};
  uint32_t edge_time_{0};
  bool last_state_{false};
  bool restore_;
};

}  // namespace esphome::duty_time_sensor
