#include "grove_ultrasonic.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::grove_ultrasonic {

static const char *const TAG = "grove_ultrasonic.sensor";

// Ignore edges within 50 µs of each other (noise filtering).
static constexpr uint32_t DEBOUNCE_US = 50;
// Ignore edges within 100 µs of the trigger pulse (filters bleed-through
// from the pulse itself).
static constexpr uint32_t START_DELAY_US = 100;
// Maximum time to wait for the echo pulse to start (~2.3 m round-trip).
static constexpr uint32_t START_TIMEOUT_US = 40000;

// ---- ISR handler -----------------------------------------------------------
void IRAM_ATTR GroveUltrasonicSensorStore::gpio_intr(GroveUltrasonicSensorStore *arg) {
  uint32_t now = micros();

  // Ignore edges after measurement is complete, or too soon after the
  // trigger pulse (the pulse's own edges fire the ISR in single-pin mode).
  if (arg->echo_end || (now - arg->measurement_start_us) <= START_DELAY_US) {
    return;
  }

  if (!arg->echo_start || (now - arg->echo_start_us) <= DEBOUNCE_US) {
    // First edge — echo has started.
    arg->echo_start_us = now;
    arg->echo_start = true;
  } else {
    // Second edge — echo has ended.
    arg->echo_end_us = now;
    arg->echo_end = true;
  }
}

// ---- Setup -----------------------------------------------------------------
void GroveUltrasonicSensorComponent::setup() {
  // Configure SIG pin as INPUT so the interrupt is ready for listening.
  this->sig_pin_->setup();
  this->sig_pin_->pin_mode(gpio::FLAG_INPUT);
  this->sig_pin_->digital_write(false);
  this->sig_pin_isr_ = this->sig_pin_->to_isr();
  this->sig_pin_->attach_interrupt(GroveUltrasonicSensorStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}

// ---- Trigger pulse ---------------------------------------------------------
void IRAM_ATTR GroveUltrasonicSensorComponent::send_trigger_pulse_() {
  InterruptLock lock;

  this->store_.echo_start = false;
  this->store_.echo_end = false;
  this->store_.measurement_start_us = micros();

  // Switch pin to OUTPUT, send the trigger pulse, then switch back to
  // INPUT.  The ISR was registered in setup() and survives the direction
  // change — the pulse edges fire the interrupt but are ignored by the
  // START_DELAY_US filter (see gpio_intr).
  this->sig_pin_isr_.pin_mode(gpio::FLAG_OUTPUT);
  this->sig_pin_isr_.digital_write(true);
  delayMicroseconds(PULSE_TIME_US);
  this->sig_pin_isr_.digital_write(false);
  this->sig_pin_isr_.pin_mode(gpio::FLAG_INPUT);

  this->measurement_pending_ = true;
  this->measurement_start_us_ = this->store_.measurement_start_us;
}

// ---- Update (called each update_interval) ----------------------------------
void GroveUltrasonicSensorComponent::update() {
  if (this->measurement_pending_) {
    return;
  }
  this->send_trigger_pulse_();
}

// ---- Loop (ticked on every main-loop iteration) ----------------------------
void GroveUltrasonicSensorComponent::loop() {
  if (!this->measurement_pending_) {
    return;
  }

  // --- Waiting for echo to start ---
  if (!this->store_.echo_start) {
    uint32_t elapsed = micros() - this->measurement_start_us_;
    if (elapsed >= START_TIMEOUT_US) {
      ESP_LOGW(TAG, "'%s' - Measurement start timed out", this->name_.c_str());
      this->publish_state(NAN);
      this->measurement_pending_ = false;
      return;
    }
  } else {
    // --- Echo has started; wait for completion or timeout ---
    uint32_t elapsed;
    if (this->store_.echo_end) {
      elapsed = this->store_.echo_end_us - this->store_.echo_start_us;
    } else {
      elapsed = micros() - this->store_.echo_start_us;
    }
    if (elapsed >= this->timeout_us_) {
      ESP_LOGD(TAG, "'%s' - Measurement pulse timed out after %" PRIu32 "us", this->name_.c_str(), elapsed);
      this->publish_state(NAN);
      this->measurement_pending_ = false;
      return;
    }
  }

  // --- Echo complete — publish distance ---
  if (this->store_.echo_end) {
    uint32_t pulse_duration = this->store_.echo_end_us - this->store_.echo_start_us;
    ESP_LOGV(TAG, "Echo took %" PRIu32 "us", pulse_duration);
    float result = GroveUltrasonicSensorComponent::us_to_m(pulse_duration);
    ESP_LOGD(TAG, "'%s' - Got distance: %.3f m", this->name_.c_str(), result);
    this->publish_state(result);
    this->measurement_pending_ = false;
  }
}

// ---- Dump config -----------------------------------------------------------
void GroveUltrasonicSensorComponent::dump_config() {
  LOG_SENSOR("", "Grove Ultrasonic Sensor", this);
  LOG_PIN("  SIG Pin: ", this->sig_pin_);
  ESP_LOGCONFIG(TAG,
                "  Pulse time: %" PRIu32 " µs\n"
                "  Timeout: %" PRIu32 " µs",
                PULSE_TIME_US, this->timeout_us_);
  LOG_UPDATE_INTERVAL(this);
}

// ---- Unit conversion -------------------------------------------------------
float GroveUltrasonicSensorComponent::us_to_m(uint32_t us) {
  const float speed_sound_m_per_s = 343.0f;
  const float time_s = us / 1e6f;
  const float total_dist = time_s * speed_sound_m_per_s;
  return total_dist / 2.0f;
}

}  // namespace esphome::grove_ultrasonic
