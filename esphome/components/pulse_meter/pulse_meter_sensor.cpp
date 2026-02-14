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

  // Set the pin value to the current value to avoid a false edge
  this->last_pin_val_ = this->pin_->digital_read();

  // Set the last processed edge to now for the first timeout
  this->last_processed_edge_us_ = micros();

  if (this->filter_mode_ == FILTER_EDGE) {
    this->pin_->attach_interrupt(PulseMeterSensor::edge_intr, this, gpio::INTERRUPT_RISING_EDGE);
  } else if (this->filter_mode_ == FILTER_PULSE) {
    // Set the pin value to the current value to avoid a false edge
    this->pulse_state_.latched_ = this->last_pin_val_;
    this->pin_->attach_interrupt(PulseMeterSensor::pulse_intr, this, gpio::INTERRUPT_ANY_EDGE);
  }

  if (this->total_sensor_ != nullptr) {
    this->total_sensor_->publish_state(this->total_pulses_);
  }
}

void PulseMeterSensor::loop() {
  State state;

  {
    // Lock the interrupt so the interrupt code doesn't interfere with itself
    InterruptLock lock;

    // Sometimes ESP devices miss interrupts if the edge rises or falls too slowly.
    // See https://github.com/espressif/arduino-esp32/issues/4172
    // If the edges are rising too slowly it also implies that the pulse rate is slow.
    // Therefore the update rate of the loop is likely fast enough to detect the edges.
    // When the main loop detects an edge that the ISR didn't it will run the ISR functions directly.
    bool current = this->pin_->digital_read();
    if (this->filter_mode_ == FILTER_EDGE && current && !this->last_pin_val_) {
      PulseMeterSensor::edge_intr(this);
    } else if (this->filter_mode_ == FILTER_PULSE && current != this->last_pin_val_) {
      PulseMeterSensor::pulse_intr(this);
    }
    this->last_pin_val_ = current;

    // Get the latest state from the ISR and reset the count in the ISR
    state.last_detected_edge_us_ = this->state_.last_detected_edge_us_;
    state.last_rising_edge_us_ = this->state_.last_rising_edge_us_;
    state.count_ = this->state_.count_;
    this->state_.count_ = 0;
  }

  const uint32_t now = micros();

  // If an edge was peeked, repay the debt
  if (this->peeked_edge_ && state.count_ > 0) {
    this->peeked_edge_ = false;
    state.count_--;
  }

  // If there is an unprocessed edge, and filter_us_ has passed since, count this edge early.
  // Wait for the debt to be repaid before counting another unprocessed edge early.
  if (!this->peeked_edge_ && state.last_rising_edge_us_ != state.last_detected_edge_us_ &&
      now - state.last_rising_edge_us_ >= this->filter_us_) {
    this->peeked_edge_ = true;
    state.last_detected_edge_us_ = state.last_rising_edge_us_;
    state.count_++;
  }

  // Check if we detected a pulse this loop
  if (state.count_ > 0) {
    // Keep a running total of pulses if a total sensor is configured
    if (this->total_sensor_ != nullptr) {
      this->total_pulses_ += state.count_;
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
        uint32_t delta_us = state.last_detected_edge_us_ - this->last_processed_edge_us_;
        float pulse_width_us = delta_us / float(state.count_);
        ESP_LOGV(TAG, "New pulse, delta: %" PRIu32 " µs, count: %" PRIu32 ", width: %.5f µs", delta_us, state.count_,
                 pulse_width_us);
        this->publish_state((60.0f * 1000000.0f) / pulse_width_us);
      } break;
    }

    this->last_processed_edge_us_ = state.last_detected_edge_us_;
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
  auto &edge_state = sensor->edge_state_;
  auto &state = sensor->state_;

  if ((now - edge_state.last_sent_edge_us_) >= sensor->filter_us_) {
    edge_state.last_sent_edge_us_ = now;
    state.last_detected_edge_us_ = now;
    state.last_rising_edge_us_ = now;
    state.count_++;  // NOLINT(clang-diagnostic-deprecated-volatile)
  }

  // This ISR is bound to rising edges, so the pin is high
  sensor->last_pin_val_ = true;
}

void IRAM_ATTR PulseMeterSensor::pulse_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method
  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();
  const bool pin_val = sensor->isr_pin_.digital_read();
  auto &pulse_state = sensor->pulse_state_;
  auto &state = sensor->state_;

  // Filter length has passed since the last interrupt
  const bool length = now - pulse_state.last_intr_ >= sensor->filter_us_;

  if (length && pulse_state.latched_ && !sensor->last_pin_val_) {  // Long enough low edge
    pulse_state.latched_ = false;
  } else if (length && !pulse_state.latched_ && sensor->last_pin_val_) {  // Long enough high edge
    pulse_state.latched_ = true;
    state.last_detected_edge_us_ = pulse_state.last_intr_;
    state.count_++;  // NOLINT(clang-diagnostic-deprecated-volatile)
  }

  // Due to order of operations this includes
  //    length && latched && rising   (just reset from a long low edge)
  //    !latched && (rising || high)  (noise on the line resetting the potential rising edge)
  state.last_rising_edge_us_ = !pulse_state.latched_ && pin_val ? now : state.last_detected_edge_us_;

  pulse_state.last_intr_ = now;
  sensor->last_pin_val_ = pin_val;
}

}  // namespace pulse_meter
}  // namespace esphome
