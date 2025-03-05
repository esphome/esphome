#include "pulse_meter_sensor.h"
#include <utility>
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_meter {

static const char *const TAG = "pulse_meter";

void PulseMeterSensor::set_total_pulses(uint32_t pulses) {
  this->total_pulses_ = pulses;
  if (this->total_sensor_ != nullptr) {
    this->total_sensor_->publish_state(this->total_pulses_);
  }
}

void PulseMeterSensor::setup() {
  this->pin_->setup();
  this->isr_pin_ = pin_->to_isr();

  // Set the last processed edge to now for the first timeout
  this->last_processed_edge_us_ = micros();

  if (this->filter_mode_ == FILTER_EDGE) {
    this->pin_->attach_interrupt(PulseMeterSensor::edge_intr, this, gpio::INTERRUPT_RISING_EDGE);
  } else if (this->filter_mode_ == FILTER_PULSE) {
    // Set the pin value to the current value to avoid a false edge
    this->pulse_state_.last_pin_val_ = this->isr_pin_.digital_read();
    this->pulse_state_.latched_ = this->pulse_state_.last_pin_val_;
    this->pin_->attach_interrupt(PulseMeterSensor::pulse_intr, this, gpio::INTERRUPT_ANY_EDGE);
  }
}

void PulseMeterSensor::loop() {
  const uint32_t now = micros();

  // Reset the count in get before we pass it back to the ISR as set
  this->get_->count_ = 0;

  // Swap out set and get to get the latest state from the ISR
  // The ISR could interrupt on any of these lines and the results would be consistent
  auto *temp = this->set_;
  this->set_ = this->get_;
  this->get_ = temp;

  // If an edge was peeked, repay the debt
  if (this->peeked_edge_ && this->get_->count_ > 0) {
    this->peeked_edge_ = false;
    this->get_->count_--;
  }

  // If there is an unprocessed edge, and filter_us_ has passed since, count this edge early
  if (this->get_->last_rising_edge_us_ != this->get_->last_detected_edge_us_ &&
      now - this->get_->last_rising_edge_us_ >= this->filter_us_) {
    this->peeked_edge_ = true;
    this->get_->last_detected_edge_us_ = this->get_->last_rising_edge_us_;
    this->get_->count_++;
  }

  // Check if we detected a pulse this loop
  if (this->get_->count_ > 0) {
    // Keep a running total of pulses if a total sensor is configured
    if (this->total_sensor_ != nullptr) {
      this->total_pulses_ += this->get_->count_;
      const uint32_t total = this->total_pulses_;
      this->total_sensor_->publish_state(total);
    }

    // We need to detect at least two edges to have a valid pulse width
    switch (this->meter_state_) {
      case MeterState::INITIAL:
      case MeterState::TIMED_OUT: {
        this->meter_state_ = MeterState::RUNNING;
      } break;
      case MeterState::RUNNING: {
        uint32_t delta_us = this->get_->last_detected_edge_us_ - this->last_processed_edge_us_;
        float pulse_width_us = delta_us / float(this->get_->count_);
        ESP_LOGV(TAG, "New pulse, delta: %" PRIu32 " µs, count: %" PRIu32 ", width: %.5f µs", delta_us,
                 this->get_->count_, pulse_width_us);
        this->publish_state((60.0f * 1000000.0f) / pulse_width_us);
      } break;
    }

    this->last_processed_edge_us_ = this->get_->last_detected_edge_us_;
  }
  // No detected edges this loop
  else {
    const uint32_t time_since_valid_edge_us = now - this->last_processed_edge_us_;

    switch (this->meter_state_) {
      // Running and initial states can timeout
      case MeterState::INITIAL:
      case MeterState::RUNNING: {
        if (time_since_valid_edge_us > this->timeout_us_) {
          this->meter_state_ = MeterState::TIMED_OUT;
          ESP_LOGD(TAG, "No pulse detected for %" PRIu32 "s, assuming 0 pulses/min",
                   time_since_valid_edge_us / 1000000);
          this->publish_state(0.0f);
        }
      } break;
      default:
        break;
    }
  }
}

float PulseMeterSensor::get_setup_priority() const { return setup_priority::DATA; }

void PulseMeterSensor::dump_config() {
  LOG_SENSOR("", "Pulse Meter", this);
  LOG_PIN("  Pin: ", this->pin_);
  if (this->filter_mode_ == FILTER_EDGE) {
    ESP_LOGCONFIG(TAG, "  Filtering rising edges less than %" PRIu32 " µs apart", this->filter_us_);
  } else {
    ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %" PRIu32 " µs", this->filter_us_);
  }
  ESP_LOGCONFIG(TAG, "  Assuming 0 pulses/min after not receiving a pulse for %" PRIu32 "s",
                this->timeout_us_ / 1000000);
}

void IRAM_ATTR PulseMeterSensor::edge_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method
  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();
  auto &state = sensor->edge_state_;
  auto &set = *sensor->set_;

  if ((now - state.last_sent_edge_us_) >= sensor->filter_us_) {
    state.last_sent_edge_us_ = now;
    set.last_detected_edge_us_ = now;
    set.last_rising_edge_us_ = now;
    set.count_++;
  }
}

void IRAM_ATTR PulseMeterSensor::pulse_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method
  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();
  const bool pin_val = sensor->isr_pin_.digital_read();
  auto &state = sensor->pulse_state_;
  auto &set = *sensor->set_;

  // Filter length has passed since the last interrupt
  const bool length = now - state.last_intr_ >= sensor->filter_us_;

  if (length && state.latched_ && !state.last_pin_val_) {  // Long enough low edge
    state.latched_ = false;
  } else if (length && !state.latched_ && state.last_pin_val_) {  // Long enough high edge
    state.latched_ = true;
    set.last_detected_edge_us_ = state.last_intr_;
    set.count_++;
  }

  // Due to order of operations this includes
  //    length && latched && rising   (just reset from a long low edge)
  //    !latched && (rising || high)  (noise on the line resetting the potential rising edge)
  set.last_rising_edge_us_ = !state.latched_ && pin_val ? now : set.last_detected_edge_us_;

  state.last_intr_ = now;
  state.last_pin_val_ = pin_val;
}

}  // namespace pulse_meter
}  // namespace esphome
