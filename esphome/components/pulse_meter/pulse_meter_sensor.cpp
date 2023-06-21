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
  this->pulse_width_us_ = 0;
  this->sensor_is_high_ = this->isr_pin_.digital_read();
  this->has_valid_edge_ = false;
  this->pending_state_change_ = NONE;
}
// In PULSE mode we set a flag (pending_state_change_) for every interrupt
// that constitutes a state change. In the loop() method we check if a time
// interval greater than the internal_filter time has passed without any
// interrupts.

void PulseMeterSensor::loop() {
  // Get a snapshot of the needed volatile sensor values, to make sure they are not
  // modified by the ISR while we are in the loop() method. If they are changed
  // after we the variable "now" has been set, overflow will occur in the
  // subsequent arithmetic
  const bool has_valid_edge = this->has_valid_edge_;
  const uint32_t last_detected_edge_us = this->last_detected_edge_us_;
  const uint32_t last_valid_edge_us = this->last_valid_edge_us_;
  // Get the current time after the snapshot of saved times
  const uint32_t now = micros();

  this->handle_state_change_(now, last_detected_edge_us, last_valid_edge_us, has_valid_edge);

  // If we've exceeded our timeout interval without receiving any pulses, assume 0 pulses/min until
  // we get at least two valid pulses.
  const uint32_t time_since_valid_edge_us = now - last_detected_edge_us;
  if ((has_valid_edge) && (time_since_valid_edge_us > this->timeout_us_)) {
    ESP_LOGD(TAG, "No pulse detected for %us, assuming 0 pulses/min", time_since_valid_edge_us / 1000000);

    this->last_valid_edge_us_ = 0;
    this->pulse_width_us_ = 0;
    this->has_valid_edge_ = false;
    this->last_detected_edge_us_ = 0;
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
  if (this->filter_mode_ == FILTER_EDGE) {
    ESP_LOGCONFIG(TAG, "  Filtering rising edges less than %u µs apart", this->filter_us_);
  } else {
    ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->filter_us_);
  }
  ESP_LOGCONFIG(TAG, "  Assuming 0 pulses/min after not receiving a pulse for %us", this->timeout_us_ / 1000000);
}

void IRAM_ATTR PulseMeterSensor::gpio_intr(PulseMeterSensor *sensor) {
  // This is an interrupt handler - we can't call any virtual method from this method
  // Get the current time before we do anything else so the measurements are consistent
  const uint32_t now = micros();
  const bool pin_val = sensor->isr_pin_.digital_read();

  if (sensor->filter_mode_ == FILTER_EDGE) {
    // We only look at rising edges
    if (!pin_val) {
      return;
    }
    // Check to see if we should filter this edge out
    if ((now - sensor->last_detected_edge_us_) >= sensor->filter_us_) {
      // Don't measure the first valid pulse (we need at least two pulses to measure the width)
      if (sensor->has_valid_edge_) {
        sensor->pulse_width_us_ = (now - sensor->last_valid_edge_us_);
      }
      sensor->total_pulses_++;
      sensor->last_valid_edge_us_ = now;
      sensor->has_valid_edge_ = true;
    }
    sensor->last_detected_edge_us_ = now;
  } else {
    // Filter Mode is PULSE
    const uint32_t delta_t_us = now - sensor->last_detected_edge_us_;
    // We need to check if we have missed to handle a state change in the
    // loop() function. This can happen when the filter_us value is less than
    // the loop() interval, which is ~50-60ms
    // The section below is essentially a modified repeat of the
    // handle_state_change method. Ideally i would refactor and call the
    // method here as well. However functions called in ISRs need to meet
    // strict criteria and I don't think the methos would meet them.
    if (sensor->pending_state_change_ != NONE && (delta_t_us > sensor->filter_us_)) {
      // We have missed to handle a state change in the loop function.
      sensor->sensor_is_high_ = sensor->pending_state_change_ == TO_HIGH;
      if (sensor->sensor_is_high_) {
        // We need to handle a pulse that would have been missed by the loop function
        sensor->total_pulses_++;
        if (sensor->has_valid_edge_) {
          sensor->pulse_width_us_ = sensor->last_detected_edge_us_ - sensor->last_valid_edge_us_;
          sensor->has_valid_edge_ = true;
          sensor->last_valid_edge_us_ = sensor->last_detected_edge_us_;
        }
      }
    }  // End of checking for and handling of change in state

    // Ignore false edges that may be caused by bouncing and exit the ISR ASAP
    if (pin_val == sensor->sensor_is_high_) {
      sensor->pending_state_change_ = NONE;
      return;
    }
    sensor->pending_state_change_ = pin_val ? TO_HIGH : TO_LOW;
    sensor->last_detected_edge_us_ = now;
  }
}

void PulseMeterSensor::handle_state_change_(uint32_t now, uint32_t last_detected_edge_us, uint32_t last_valid_edge_us,
                                            bool has_valid_edge) {
  if (this->pending_state_change_ == NONE) {
    return;
  }

  const bool pin_val = this->isr_pin_.digital_read();
  if (pin_val == this->sensor_is_high_) {
    // Most likely caused by high frequency bouncing. Theoretically we should
    // expect interrupts of alternating state. Here we are registering an
    // interrupt with no change in state. Another interrupt will likely trigger
    // just after this one and have an alternate state.
    this->pending_state_change_ = NONE;
    return;
  }

  if ((now - last_detected_edge_us) > this->filter_us_) {
    this->sensor_is_high_ = pin_val;
    ESP_LOGVV(TAG, "State is now %s", pin_val ? "high" : "low");

    // Increment with valid rising edges only
    if (pin_val) {
      this->total_pulses_++;
      ESP_LOGVV(TAG, "Incremented pulses to %u", this->total_pulses_);

      if (has_valid_edge) {
        this->pulse_width_us_ = last_detected_edge_us - last_valid_edge_us;
        ESP_LOGVV(TAG, "Set pulse width to %u", this->pulse_width_us_);
      }
      this->has_valid_edge_ = true;
      this->last_valid_edge_us_ = last_detected_edge_us;
      ESP_LOGVV(TAG, "last_valid_edge_us_ is now %u", this->last_valid_edge_us_);
    }
    this->pending_state_change_ = NONE;
  }
}

}  // namespace pulse_meter
}  // namespace esphome
