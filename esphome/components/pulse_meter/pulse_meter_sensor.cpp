#include "pulse_meter_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_meter {

static const char *TAG = "pulse_meter";

void PulseMeterSensorStore::setup(GPIOPin *pin) {
  pin->setup();
  this->isr_pin = pin->to_isr();
  pin->attach_interrupt(PulseMeterSensorStore::gpio_intr, this, CHANGE);

  const uint32_t now = micros();
  this->last_detected_edge_us = now;
  this->last_valid_edge_us = now;
}

uint32_t PulseMeterSensorStore::get_filter_us() const {
  return this->filter_us;
}

void PulseMeterSensorStore::set_filter_us(uint32_t filter) {
  this->filter_us = filter;
}

uint32_t PulseMeterSensorStore::get_pulse_width_ms() const {
  return this->pulse_width_ms;
}

uint32_t PulseMeterSensorStore::get_total_pulses() const {
  return this->total_pulses;
}

void ICACHE_RAM_ATTR PulseMeterSensorStore::gpio_intr(PulseMeterSensorStore *store) {
  const uint32_t now = micros();

  // We only look at rising edges
  if (store->isr_pin->digital_read()) {
    // Check to see if we should filter this edge out
    if ((now - store->last_detected_edge_us) >= store->filter_us) {
      // This looks like a valid pulse. 
      store->total_pulses++;

      // We store the pulse width in ms to avoid wrapping with very slow pulses (>71 
      // mins/pulse) at the cost of under-reading at very high frequencies. In 
      // practice, for a typical electricity meter that outputs 1 pulse per Wh, 1 
      // ms pulses would be ~3.6 MW, this is an unlikely situation. Very low 
      // usage is more likely in a domestic setup (e.g. an empty house with 
      // all appliances turned off).
      store->pulse_width_ms = (now - store->last_valid_edge_us) / 1000;

      store->last_valid_edge_us = now;
    }

    store->last_detected_edge_us = now;
  }
}

void PulseMeterSensor::set_pin(GPIOPin *pin) {
  this->pin = pin;
}

void PulseMeterSensor::set_filter_us(uint32_t filter) {
  this->storage.set_filter_us(filter);
}

void PulseMeterSensor::set_total_sensor(sensor::Sensor *sensor) {
  this->total_sensor = sensor;
}

void PulseMeterSensor::setup() {
  this->storage.setup(this->pin);
}

void PulseMeterSensor::loop() {
  const uint32_t pulse_width_ms = this->storage.get_pulse_width_ms();
  if (this->pulse_width_dedupe_.next(pulse_width_ms)) {
    if (pulse_width_ms == 0) {
      this->publish_state(0);
    } else {
      this->publish_state((60.0 * 1000.0) / pulse_width_ms);
    }
  }

  if (this->total_sensor != nullptr) {
    const uint32_t total = storage.get_total_pulses();
    if (this->total_dedupe_.next(total)) {
      this->total_sensor->publish_state(total);
    }   
  }
}

void PulseMeterSensor::dump_config() {
  LOG_SENSOR("", "Pulse Meter", this);
  LOG_PIN("  Pin: ", this->pin);
  ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->storage.get_filter_us());
}

}  // namespace pulse_counter
}  // namespace esphome
