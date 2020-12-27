#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace pulse_meter {

/// Store data in a non-virtual class to avoid issues with vtables in ISRs
class PulseMeterSensorStore {
 public:
  void setup(GPIOPin *pin);

  uint32_t get_filter_us() const;
  void set_filter_us(uint32_t filter);

  uint32_t get_pulse_width_ms() const;
  uint32_t get_total_pulses() const;

 protected:
  static void gpio_intr(PulseMeterSensorStore *arg);

  ISRInternalGPIOPin *isr_pin;
  uint32_t filter_us = 0;

  volatile uint32_t last_detected_edge_us = 0;
  volatile uint32_t last_valid_edge_us = 0;
  volatile uint32_t pulse_width_ms = 0;
  volatile uint32_t total_pulses = 0;
};

class PulseMeterSensor : public sensor::Sensor, public Component {
 public:
  void set_pin(GPIOPin *pin);
  void set_filter_us(uint32_t filter);
  void set_total_sensor(sensor::Sensor *sensor);

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  GPIOPin *pin = nullptr;
  PulseMeterSensorStore storage;
  sensor::Sensor *total_sensor = nullptr;
  Deduplicator<uint32_t> pulse_width_dedupe_;
  Deduplicator<uint32_t> total_dedupe_;
};

}  // namespace pulse_meter
}  // namespace esphome
