#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace total_daily_energy {

enum TotalDailyEnergyMethod {
  TOTAL_DAILY_ENERGY_METHOD_TRAPEZOID = 0,
  TOTAL_DAILY_ENERGY_METHOD_LEFT,
  TOTAL_DAILY_ENERGY_METHOD_RIGHT,
};

class TotalDailyEnergy : public sensor::Sensor, public Component {
 public:
  void set_restore(bool restore) { restore_ = restore; }
  void set_time(time::RealTimeClock *time) { time_ = time; }
  void set_parent(Sensor *parent) { parent_ = parent; }
  void set_method(TotalDailyEnergyMethod method) { method_ = method; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

  void publish_state_and_save(float state);

 protected:
  void process_new_state_(float state);

  ESPPreferenceObject pref_;
  time::RealTimeClock *time_;
  Sensor *parent_;
  TotalDailyEnergyMethod method_;
  uint16_t last_day_of_year_{};
  uint32_t last_update_{0};
  bool restore_;
  float total_energy_{0.0f};
  float last_power_state_{0.0f};
};

}  // namespace total_daily_energy
}  // namespace esphome
