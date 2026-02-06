#include "ultrasonic_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ultrasonic {

static const char *const TAG = "ultrasonic.sensor";

static constexpr uint32_t START_TIMEOUT_US = 40000;  // Maximum time to wait for echo pulse to start

void IRAM_ATTR UltrasonicSensorStore::gpio_intr(UltrasonicSensorStore *arg) {
  uint32_t now = micros();
  if (arg->echo_pin_isr.digital_read()) {
    arg->echo_start_us = now;
    arg->echo_start = true;
  } else {
    arg->echo_end_us = now;
    arg->echo_end = true;
  }
}

void IRAM_ATTR UltrasonicSensorComponent::send_trigger_pulse_() {
  InterruptLock lock;
  this->store_.echo_start_us = 0;
  this->store_.echo_end_us = 0;
  this->store_.echo_start = false;
  this->store_.echo_end = false;
  this->trigger_pin_isr_.digital_write(true);
  delayMicroseconds(this->pulse_time_us_);
  this->trigger_pin_isr_.digital_write(false);
  this->measurement_pending_ = true;
  this->measurement_start_us_ = micros();
}

void UltrasonicSensorComponent::setup() {
  this->trigger_pin_->setup();
  this->trigger_pin_->digital_write(false);
  this->trigger_pin_isr_ = this->trigger_pin_->to_isr();
  this->echo_pin_->setup();
  this->store_.echo_pin_isr = this->echo_pin_->to_isr();
  this->echo_pin_->attach_interrupt(UltrasonicSensorStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}

void UltrasonicSensorComponent::update() {
  if (this->measurement_pending_) {
    return;
  }
  this->send_trigger_pulse_();
}

void UltrasonicSensorComponent::loop() {
  if (!this->measurement_pending_) {
    return;
  }

  if (!this->store_.echo_start) {
    uint32_t elapsed = micros() - this->measurement_start_us_;
    if (elapsed >= START_TIMEOUT_US) {
      ESP_LOGW(TAG, "'%s' - Measurement start timed out", this->name_.c_str());
      this->publish_state(NAN);
      this->measurement_pending_ = false;
      return;
    }
  } else {
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

  if (this->store_.echo_end) {
    float result;
    if (this->store_.echo_start) {
      uint32_t pulse_duration = this->store_.echo_end_us - this->store_.echo_start_us;
      ESP_LOGV(TAG, "pulse start took %" PRIu32 "us, echo took %" PRIu32 "us",
               this->store_.echo_start_us - this->measurement_start_us_, pulse_duration);
      result = UltrasonicSensorComponent::us_to_m(pulse_duration);
      ESP_LOGD(TAG, "'%s' - Got distance: %.3f m", this->name_.c_str(), result);
    } else {
      ESP_LOGW(TAG, "'%s' - pulse end before pulse start, does the echo pin need to be inverted?", this->name_.c_str());
      result = NAN;
    }
    this->publish_state(result);
    this->measurement_pending_ = false;
    return;
  }
}

void UltrasonicSensorComponent::dump_config() {
  LOG_SENSOR("", "Ultrasonic Sensor", this);
  LOG_PIN("  Echo Pin: ", this->echo_pin_);
  LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
  ESP_LOGCONFIG(TAG,
                "  Pulse time: %" PRIu32 " µs\n"
                "  Timeout: %" PRIu32 " µs",
                this->pulse_time_us_, this->timeout_us_);
  LOG_UPDATE_INTERVAL(this);
}

float UltrasonicSensorComponent::us_to_m(uint32_t us) {
  const float speed_sound_m_per_s = 343.0f;
  const float time_s = us / 1e6f;
  const float total_dist = time_s * speed_sound_m_per_s;
  return total_dist / 2.0f;
}

}  // namespace esphome::ultrasonic
