#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace duty_time_sensor {

class DutyTimeSensor : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

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

  uint32_t total_sec_;
  uint32_t last_time_;
  uint32_t edge_time_;
  bool last_state_{false};
  bool restore_;
};

template<typename... Ts> class BaseAction : public Action<Ts...>, public Parented<DutyTimeSensor> {};

template<typename... Ts> class StartAction : public BaseAction<Ts...> {
  void play(Ts... x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public BaseAction<Ts...> {
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts> class ResetAction : public BaseAction<Ts...> {
  void play(Ts... x) override { this->parent_->reset(); }
};

template<typename... Ts> class RunningCondition : public Condition<Ts...>, public Parented<DutyTimeSensor> {
 public:
  explicit RunningCondition(DutyTimeSensor *parent, bool state) : Parented(parent), state_(state) {}

 protected:
  bool check(Ts... x) override { return this->parent_->is_running() == this->state_; }
  bool state_;
};

}  // namespace duty_time_sensor
}  // namespace esphome
