#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <cinttypes>

namespace esphome::pulse_meter {

class PulseMeterSensor final : public sensor::Sensor, public Component {
  SUB_SENSOR(total)

 public:
  enum InternalFilterMode : uint8_t {
    FILTER_EDGE = 0,
    FILTER_PULSE,
  };

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_filter_us(uint32_t filter) { this->filter_us_ = filter; }
  void set_timeout_us(uint32_t timeout) { this->timeout_us_ = timeout; }
  void set_filter_mode(InternalFilterMode mode) { this->filter_mode_ = mode; }

  void set_total_pulses(uint32_t pulses);

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  static void edge_intr(PulseMeterSensor *sensor);
  static void pulse_intr(PulseMeterSensor *sensor);

  InternalGPIOPin *pin_{nullptr};
  uint32_t filter_us_ = 0;
  uint32_t timeout_us_ = 1000000UL * 60UL * 5UL;

  // Variables used in the loop
  uint32_t total_pulses_ = 0;
  uint32_t last_processed_edge_us_ = 0;

  // This struct and variable are used to pass data between the ISR and loop.
  // The data from state_ is read and then count_ in state_ is reset in each loop.
  // This must be done while guarded by an InterruptLock. Use this variable to send data
  // from the ISR to the loop not the other way around (except for resetting count_).
  struct State {
    uint32_t last_detected_edge_us_ = 0;
    uint32_t last_rising_edge_us_ = 0;
    uint32_t count_ = 0;
  };
  volatile State state_{};

  // Only use the following variables in the ISR or while guarded by an InterruptLock
  ISRInternalGPIOPin isr_pin_;

  /// Filter state for edge mode
  struct EdgeState {
    uint32_t last_sent_edge_us_ = 0;
  };
  EdgeState edge_state_{};

  /// Filter state for pulse mode
  struct PulseState {
    uint32_t last_intr_ = 0;
    bool latched_ = false;
  };
  PulseState pulse_state_{};

  // Byte fields packed together; last_pin_val_ (the last pin value seen) is ISR or InterruptLock only
  enum class MeterState : uint8_t { INITIAL, RUNNING, TIMED_OUT };
  InternalFilterMode filter_mode_{FILTER_EDGE};
  MeterState meter_state_ = MeterState::INITIAL;
  bool peeked_edge_ = false;
  bool last_pin_val_ = false;
};

}  // namespace esphome::pulse_meter
