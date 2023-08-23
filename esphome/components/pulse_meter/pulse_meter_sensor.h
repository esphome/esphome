#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace pulse_meter {

class PulseMeterSensor : public sensor::Sensor, public Component {
 public:
  enum InternalFilterMode {
    FILTER_EDGE = 0,
    FILTER_PULSE,
  };

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_filter_us(uint32_t filter) { this->filter_us_ = filter; }
  void set_timeout_us(uint32_t timeout) { this->timeout_us_ = timeout; }
  void set_total_sensor(sensor::Sensor *sensor) { this->total_sensor_ = sensor; }
  void set_filter_mode(InternalFilterMode mode) { this->filter_mode_ = mode; }
  void set_total_pulses(uint32_t pulses) { this->total_pulses_ = pulses; }

  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  static void edge_intr(PulseMeterSensor *sensor);
  static void pulse_intr(PulseMeterSensor *sensor);

  InternalGPIOPin *pin_{nullptr};
  uint32_t filter_us_ = 0;
  uint32_t timeout_us_ = 1000000UL * 60UL * 5UL;
  sensor::Sensor *total_sensor_{nullptr};
  InternalFilterMode filter_mode_{FILTER_EDGE};

  // Variables used in the loop
  bool initialized_ = false;
  uint32_t total_pulses_ = 0;
  uint32_t last_processed_edge_us_ = 0;

  // This struct (and the two pointers) are used to pass data between the ISR and loop.
  // These two pointers are exchanged each loop.
  // Therefore you can't use data in the pointer to loop receives to set values in the pointer to loop sends.
  // As a result it's easiest if you only use these pointers to send data from the ISR to the loop.
  // (except for resetting the values)
  struct State {
    uint32_t last_detected_edge_us_ = 0;
    uint32_t count_ = 0;
  };
  State state_[2];
  volatile State *set_ = state_;
  volatile State *get_ = state_ + 1;

  // Only use these variables in the ISR
  ISRInternalGPIOPin isr_pin_;
  uint32_t last_edge_candidate_us_ = 0;
  uint32_t last_intr_ = 0;
  bool in_pulse_ = false;
  bool last_pin_val_ = false;
};

}  // namespace pulse_meter
}  // namespace esphome
