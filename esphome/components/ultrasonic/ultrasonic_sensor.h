#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome::ultrasonic {

struct UltrasonicSensorStore {
  static void gpio_intr(UltrasonicSensorStore *arg);

  volatile uint32_t echo_start_us{0};
  volatile uint32_t echo_end_us{0};
  volatile bool echo_start{false};
  volatile bool echo_end{false};
};

class UltrasonicSensorComponent : public sensor::Sensor, public PollingComponent {
 public:
  void set_trigger_pin(InternalGPIOPin *trigger_pin) { this->trigger_pin_ = trigger_pin; }
  void set_echo_pin(InternalGPIOPin *echo_pin) { this->echo_pin_ = echo_pin; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  /// Set the time in µs the trigger pin should be enabled for in µs, defaults to 10µs (for HC-SR04)
  void set_pulse_time_us(uint32_t pulse_time_us) { this->pulse_time_us_ = pulse_time_us; }

 protected:
  /// Helper function to convert the specified echo duration in µs to meters.
  static float us_to_m(uint32_t us);
  void send_trigger_pulse_();

  InternalGPIOPin *trigger_pin_;
  ISRInternalGPIOPin trigger_pin_isr_;
  InternalGPIOPin *echo_pin_;
  UltrasonicSensorStore store_;
  uint32_t pulse_time_us_{};

  uint32_t measurement_start_us_{0};
  bool measurement_pending_{false};
};

}  // namespace esphome::ultrasonic
