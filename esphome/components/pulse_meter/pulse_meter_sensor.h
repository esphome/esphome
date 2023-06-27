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
  void set_filter_mode(InternalFilterMode mode) { this->filter_mode_ = mode; }
  void set_timeout_us(uint32_t timeout) { this->timeout_us_ = timeout; }
  void set_total_sensor(sensor::Sensor *sensor) { this->total_sensor_ = sensor; }

  void set_total_pulses(uint32_t pulses);

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  enum StateChange { TO_LOW = 0, TO_HIGH, NONE };

  static void gpio_intr(PulseMeterSensor *sensor);
  void handle_state_change_(uint32_t now, uint32_t last_detected_edge_us, uint32_t last_valid_edge_us,
                            bool has_valid_edge);

  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin isr_pin_;
  uint32_t filter_us_ = 0;
  uint32_t timeout_us_ = 1000000UL * 60UL * 5UL;
  sensor::Sensor *total_sensor_{nullptr};
  InternalFilterMode filter_mode_{FILTER_EDGE};

  Deduplicator<uint32_t> pulse_width_dedupe_;
  Deduplicator<uint32_t> total_dedupe_;

  volatile uint32_t last_detected_edge_us_ = 0;
  volatile uint32_t last_valid_edge_us_ = 0;
  volatile uint32_t pulse_width_us_ = 0;
  volatile uint32_t total_pulses_ = 0;
  volatile bool sensor_is_high_ = false;
  volatile bool has_valid_edge_ = false;
  volatile StateChange pending_state_change_{NONE};
};

}  // namespace pulse_meter
}  // namespace esphome
