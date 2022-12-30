#include "pulse_meter_sensor.h"
#include <utility>
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_meter {

static const char *const TAG = "pulse_meter";

void PulseMeterSensor::set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
void PulseMeterSensor::set_filter_us(uint32_t filter) { this->filter_us_ = filter; }
void PulseMeterSensor::set_timeout_us(uint32_t timeout) { this->timeout_us_ = timeout; }
void PulseMeterSensor::set_total_sensor(sensor::Sensor *sensor) { this->total_sensor_ = sensor; }
void PulseMeterSensor::set_filter_mode(InternalFilterMode mode) { this->filter_mode_ = mode; }

void PulseMeterSensor::set_total_pulses(uint32_t pulses) { this->total_pulses_ = pulses; }

void PulseMeterSensor::setup() {
  this->pin_->setup();
  this->isr_pin_ = pin_->to_isr();

  last_edge_candidate_us_ = 0;
  // We must start with a low value to detect the first rising edge
  this->last_pin_val_ = false;

  this->initialized_ = false;

  this->get_->count_ = 0;
  this->get_->last_detected_edge_us_ = 0;
  this->set_->count_ = 0;
  this->set_->last_detected_edge_us_ = 0;

  this->get_ = this->state_;
  this->set_ = this->state_ + 1;

  if (this->filter_mode_ == FILTER_EDGE) {
    this->pin_->attach_interrupt(PulseMeterSensor::edge_intr, this, gpio::INTERRUPT_RISING_EDGE);
  } else if (this->filter_mode_ == FILTER_PULSE) {
    this->pin_->attach_interrupt(PulseMeterSensor::pulse_intr, this, gpio::INTERRUPT_ANY_EDGE);
  }
}

void PulseMeterSensor::loop() {
  // Reset the count in get before we pass it back to the ISR as set
  this->get_->count_ = 0;

  // Swap out set and get to get the latest state from the ISR
  // The ISR could interrupt on any of these lines and the results would be consistent
  auto *temp = this->set_;
  this->set_ = this->get_;
  this->get_ = temp;

  // Check if we detected a pulse this loop
  if (this->get_->count_ > 0) {
    // Keep a running total of pulses if a total sensor is configured
    if (this->total_sensor_ != nullptr) {
      this->total_pulses_ += this->get_->count_;
      const uint32_t total = this->total_pulses_;
      this->total_sensor_->publish_state(total);
    }

    // We need to detect at least two edges to have a valid pulse width
    if (!this->initialized_) {
      this->initialized_ = true;
    } else {
      uint32_t delta_us = this->get_->last_detected_edge_us_ - this->last_processed_edge_us_;
      float pulse_width_us = delta_us / float(this->get_->count_);
      this->publish_state((60.0f * 1000000.0f) / pulse_width_us);
    }

    this->last_processed_edge_us_ = this->get_->last_detected_edge_us_;
  }
  // No detected edges this loop
  else {
    const uint32_t now = micros();
    const uint32_t time_since_valid_edge_us = now - this->last_processed_edge_us_;

    if (this->initialized_ && time_since_valid_edge_us > this->timeout_us_) {
      ESP_LOGD(TAG, "No pulse detected for %us, assuming 0 pulses/min", time_since_valid_edge_us / 1000000);
      this->initialized_ = false;
      this->publish_state(0.0f);
    }
  }
}

float PulseMeterSensor::get_setup_priority() const { return setup_priority::DATA; }

void PulseMeterSensor::dump_config() {
  LOG_SENSOR("", "Pulse Meter", this);
  LOG_PIN("  Pin: ", this->pin_);
  if (this->filter_mode_ == FILTER_EDGE) {
    ESP_LOGCONFIG(TAG, "  Filtering rising edges less than %u µs apart", this->filter_us_);
  } else {
    ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->filter_us_);
  }
  ESP_LOGCONFIG(TAG, "  Assuming 0 pulses/min after not receiving a pulse for %us", this->timeout_us_ / 1000000);
}

void IRAM_ATTR PulseMeterSensor::edge_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method
  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();

  if ((now - sensor->last_edge_candidate_us_) >= sensor->filter_us_) {
    sensor->last_edge_candidate_us_ = now;
    sensor->set_->last_detected_edge_us_ = now;
    sensor->set_->count_++;
  }
}

void IRAM_ATTR PulseMeterSensor::pulse_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method
  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();
  const bool pin_val = sensor->isr_pin_.digital_read();

  // Rising edge
  if (!sensor->last_pin_val_ && pin_val) {
    sensor->last_edge_candidate_us_ = now;
  }
  // Falling edge
  else if (sensor->last_pin_val_ && !pin_val) {
    // Check that the length of this pulse is greater than the filter time
    if (now - sensor->last_edge_candidate_us_ > sensor->filter_us_) {
      sensor->set_->last_detected_edge_us_ = sensor->last_edge_candidate_us_;
      sensor->set_->count_++;
    }
  }
  // We were in a pulse and the pin is still high, a dropout must have happened faster than we could measure
  // If we hadn't yet reached our filter time, reset the candidate to now
  // However if we already reached our filter time ignore it as noise
  else if (sensor->last_pin_val_ && pin_val) {
    if (now - sensor->last_edge_candidate_us_ < sensor->filter_us_) {
      sensor->last_edge_candidate_us_ = now;
    }
  } else {
    // We were not in a pulse and the pin is still low, a pulse must have occurred faster than we could measure
  }
  sensor->last_pin_val_ = pin_val;
}

}  // namespace pulse_meter
}  // namespace esphome
