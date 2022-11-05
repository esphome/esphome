#include "pulse_meter_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_meter {

static const char *const TAG = "pulse_meter";

void PulseMeterSensor::setup() {
  this->pin_->setup();
  this->isr_pin_ = pin_->to_isr();
  this->pin_->attach_interrupt(PulseMeterSensor::gpio_intr, this, gpio::INTERRUPT_ANY_EDGE);

  this->pulse_width_us_ = 0;
  this->last_detected_edge_us_ = 0;
  this->last_valid_high_edge_us_ = 0;
  this->last_valid_low_edge_us_ = 0;
  this->sensor_is_high_ = this->isr_pin_.digital_read();
  this->has_valid_high_edge_ = false;
  this->has_valid_low_edge_ = false;
}

void PulseMeterSensor::loop() {
  // Get a local copy of the volatile sensor values, to make sure they are not
  // modified by the ISR. This could cause overflow in the following arithmetic
  const uint32_t last_valid_high_edge_us = this->last_valid_high_edge_us_;
  const bool has_valid_high_edge = this->has_valid_high_edge_;
  const uint32_t now = micros();

  // If we've exceeded our timeout interval without receiving any pulses, assume
  // 0 pulses/min until we get at least two valid pulses.
  const uint32_t time_since_valid_edge_us = now - last_valid_high_edge_us;
  if ((has_valid_high_edge) && (time_since_valid_edge_us > this->timeout_us_)) {
    ESP_LOGD(TAG, "No pulse detected for %us, assuming 0 pulses/min", time_since_valid_edge_us / 1000000);

    this->pulse_width_us_ = 0;
    this->last_detected_edge_us_ = 0;
    this->last_valid_high_edge_us_ = 0;
    this->last_valid_low_edge_us_ = 0;
    this->has_detected_edge_ = false;
    this->has_valid_high_edge_ = false;
    this->has_valid_low_edge_ = false;
  }

  // We quantize our pulse widths to 1 ms to avoid unnecessary jitter
  const uint32_t pulse_width_ms = this->pulse_width_us_ / 1000;
  if (this->pulse_width_dedupe_.next(pulse_width_ms)) {
    if (pulse_width_ms == 0) {
      // Treat 0 pulse width as 0 pulses/min (normally because we've not
      // detected any pulses for a while)
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
  if (this->filter_mode_ == FILTER_EDGE) {
    ESP_LOGCONFIG(TAG, "  Filtering rising edges less than %u µs apart", this->filter_us_);
  } else {
    ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->filter_us_);
  }
  ESP_LOGCONFIG(TAG, "  Assuming 0 pulses/min after not receiving a pulse for %us", this->timeout_us_ / 1000000);
}

void IRAM_ATTR PulseMeterSensor::gpio_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this
  // method

  // Get the current time before we do anything else so the measurements are
  // consistent
  const uint32_t now = micros();

  // We only look at rising edges in EDGE mode, and all edges in PULSE mode
  if (sensor->filter_mode_ == FILTER_EDGE) {
    if (sensor->isr_pin_.digital_read()) {
      sensor->last_detected_edge_us_ = now;
    }
  }

  // Check to see if we should filter this edge out
  if (sensor->filter_mode_ == FILTER_EDGE) {
    if ((sensor->last_detected_edge_us_ - sensor->last_valid_high_edge_us_) >= sensor->filter_us_) {
      // Don't measure the first valid pulse (we need at least two pulses to
      // measure the width)
      if (sensor->has_valid_high_edge_) {
        sensor->pulse_width_us_ = (sensor->last_detected_edge_us_ - sensor->last_valid_high_edge_us_);
      }
      sensor->total_pulses_++;
      sensor->last_valid_high_edge_us_ = sensor->last_detected_edge_us_;
      sensor->has_valid_high_edge_ = true;
    }
  } else {
    // Filter Mode is PULSE
    bool pin_val = sensor->isr_pin_.digital_read();
    // Ignore false edges that may be caused by bouncing and exit the ISR ASAP
    if (pin_val == sensor->sensor_is_high_) {
      return;
    }
    // Make sure the signal has been stable long enough
    if (sensor->has_detected_edge_ && (now - sensor->last_detected_edge_us_ >= sensor->filter_us_)) {
      if (pin_val) {
        sensor->has_valid_high_edge_ = true;
        sensor->last_valid_high_edge_us_ = sensor->last_detected_edge_us_;
        sensor->sensor_is_high_ = true;
      } else {
        // Count pulses when a sufficiently long high pulse is concluded.
        sensor->total_pulses_++;
        if (sensor->has_valid_low_edge_) {
          sensor->pulse_width_us_ = sensor->last_detected_edge_us_ - sensor->last_valid_low_edge_us_;
        }
        sensor->has_valid_low_edge_ = true;
        sensor->last_valid_low_edge_us_ = sensor->last_detected_edge_us_;
        sensor->sensor_is_high_ = false;
      }
    }
    sensor->has_detected_edge_ = true;
    sensor->last_detected_edge_us_ = now;
  }
}

}  // namespace pulse_meter
}  // namespace esphome
