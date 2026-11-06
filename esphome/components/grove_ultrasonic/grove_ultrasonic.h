#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome::grove_ultrasonic {

/// ISR-safe storage for echo pulse timing.
struct GroveUltrasonicSensorStore {
  static void gpio_intr(GroveUltrasonicSensorStore *arg);

  volatile uint32_t measurement_start_us{0};
  volatile uint32_t echo_start_us{0};
  volatile uint32_t echo_end_us{0};
  volatile bool echo_start{false};
  volatile bool echo_end{false};
};

/// Grove Ultrasonic Ranger — single-pin sensor.
///
/// Uses a shared SIG pin that alternates between OUTPUT (trigger
/// pulse) and INPUT (echo listening).  Configure with ``sig_pin:``
/// in YAML.
class GroveUltrasonicSensorComponent : public sensor::Sensor, public PollingComponent {
 public:
  void set_sig_pin(InternalGPIOPin *pin) { this->sig_pin_ = pin; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

  /// Set the maximum time in µs to wait for the echo to return.
  void set_timeout_us(uint32_t timeout_us) { this->timeout_us_ = timeout_us; }

 protected:
  /// Trigger pulse width in µs (standard for Grove / HC-SR04).
  static constexpr uint32_t PULSE_TIME_US = 10;
  /// Convert an echo duration in µs to distance in metres.
  static float us_to_m(uint32_t us);
  /// Send the trigger pulse, switching pin direction as needed.
  void send_trigger_pulse_();

  InternalGPIOPin *sig_pin_{nullptr};
  ISRInternalGPIOPin sig_pin_isr_;
  GroveUltrasonicSensorStore store_;
  uint32_t timeout_us_{};

  uint32_t measurement_start_us_{0};
  bool measurement_pending_{false};
};

}  // namespace esphome::grove_ultrasonic
