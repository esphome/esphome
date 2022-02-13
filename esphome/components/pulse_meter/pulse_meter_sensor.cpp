#include "pulse_meter_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_meter {

static const char *const TAG = "pulse_meter";

void PulseMeterSensor::setup() {
  this->pin_->setup();
  this->isr_pin_ = pin_->to_isr();
  this->pin_->attach_interrupt(PulseMeterSensor::gpio_intr, this, gpio::INTERRUPT_ANY_EDGE);

  this->last_detected_edge_us_ = 0;
  this->last_valid_edge_us_ = 0;
}

void PulseMeterSensor::loop() {
  const uint32_t now = micros();

  // If we've exceeded our timeout interval without receiving any pulses, assume 0 pulses/min until
  // we get at least two valid pulses.
  const uint32_t time_since_valid_edge_us = now - this->last_valid_edge_us_;
  if ((this->last_valid_edge_us_ != 0) && (time_since_valid_edge_us > this->timeout_us_)) {
    ESP_LOGD(TAG, "No pulse detected for %us, assuming 0 pulses/min", time_since_valid_edge_us / 1000000);
    this->last_valid_edge_us_ = 0;
    this->pulse_width_us_ = 0;
  }

  // We quantize our pulse widths to 1 ms to avoid unnecessary jitter
  const uint32_t pulse_width_ms = this->pulse_width_us_ / 1000;
  if (this->pulse_width_dedupe_.next(pulse_width_ms)) {
    if (pulse_width_ms == 0) {
      // Treat 0 pulse width as 0 pulses/min (normally because we've not detected any pulses for a while)
      this->publish_state(0);
    } else {
      // Calculate pulses/min from the pulse width in ms
      this->publish_state((60.0f * 1000.0f) / pulse_width_ms);
    }
  }

  if (this->total_sensor_ != nullptr) {
    const uint32_t total = this->total_pulses_;
    if (this->total_dedupe_.next(total)) {
      this->total_sensor_->publish_state(total);
    }
  }
}

void PulseMeterSensor::set_total_pulses(uint32_t pulses) { this->total_pulses_ = pulses; }

void PulseMeterSensor::dump_config() {
  LOG_SENSOR("", "Pulse Meter", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->filter_us_);
  ESP_LOGCONFIG(TAG, "  Assuming 0 pulses/min after not receiving a pulse for %us", this->timeout_us_ / 1000000);
}

void IRAM_ATTR PulseMeterSensor::gpio_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method

  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();

  // We only look at rising edges
  if (!sensor->isr_pin_.digital_read()) {
    return;
  }

  // Check to see if we should filter this edge out
  if ((now - sensor->last_detected_edge_us_) >= sensor->filter_us_) {
    // Don't measure the first valid pulse (we need at least two pulses to measure the width)
    if (sensor->last_valid_edge_us_ != 0) {
      sensor->pulse_width_us_ = (now - sensor->last_valid_edge_us_);
    }

    sensor->total_pulses_++;
    sensor->last_valid_edge_us_ = now;
  }

  sensor->last_detected_edge_us_ = now;
}

}  // namespace pulse_meter
}  // namespace esphome
