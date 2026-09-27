#pragma once

#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::counter {

/// A counter held as a 64-bit integer. The published sensor state is a float, so it is exact only up to 2^24.
class CounterSensor final : public sensor::Sensor, public Component {
 public:
  /// The counter starts at initial_value unless a stored value is restored.
  CounterSensor(bool restore, int64_t initial_value) : value_(initial_value), restore_(restore) {}

  void setup() override;
  void dump_config() override;
  // restore value before `on_boot` automations run
  float get_setup_priority() const override { return setup_priority::DATA - 50.0f; }

  /// Increment by one each time the given sensor publishes a state.
  void count_updates_from(sensor::Sensor *source) {
    source->add_on_state_callback([this](float) { this->increment(); });
  }

  /// Increment by one each time the given binary sensor changes to true.
  template<typename T> void count_true_from(T *source) {
    source->add_on_state_callback([this](bool state) {
      if (state)
        this->increment();
    });
  }

  void set_value(int64_t value);
  /// overflow on addition of signed numbers is undefined - use the well defined unsigned version
  void increment(int64_t amount = 1) {
    this->set_value(static_cast<int64_t>(static_cast<uint64_t>(this->value_) + static_cast<uint64_t>(amount)));
  }

 protected:
  ESPPreferenceObject pref_;
  int64_t value_;
  bool restore_;
};

}  // namespace esphome::counter
