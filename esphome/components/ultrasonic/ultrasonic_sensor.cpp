#include "ultrasonic_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ultrasonic {

static const char *const TAG = "ultrasonic.sensor";

static constexpr uint32_t DEBOUNCE_US = 50;                // Ignore edges within 50us (noise filtering)
static constexpr uint32_t MEASUREMENT_TIMEOUT_US = 80000;  // Maximum time to wait for measurement completion

void IRAM_ATTR UltrasonicSensorStore::gpio_intr(UltrasonicSensorStore *arg) {
  uint32_t now = micros();
  if (!arg->echo_start || (now - arg->echo_start_us) <= DEBOUNCE_US) {
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

  if (this->store_.echo_end) {
    uint32_t pulse_duration = this->store_.echo_end_us - this->store_.echo_start_us;
    ESP_LOGV(TAG, "Echo took %" PRIu32 "us", pulse_duration);
    float result = UltrasonicSensorComponent::us_to_m(pulse_duration);
    ESP_LOGD(TAG, "'%s' - Got distance: %.3f m", this->name_.c_str(), result);
    this->publish_state(result);
    this->measurement_pending_ = false;
    return;
  }

  uint32_t elapsed = micros() - this->measurement_start_us_;
  if (elapsed >= MEASUREMENT_TIMEOUT_US) {
    ESP_LOGD(TAG, "'%s' - Measurement timed out after %" PRIu32 "us", this->name_.c_str(), elapsed);
    this->publish_state(NAN);
    this->measurement_pending_ = false;
  }
}

void UltrasonicSensorComponent::dump_config() {
  LOG_SENSOR("", "Ultrasonic Sensor", this);
  LOG_PIN("  Echo Pin: ", this->echo_pin_);
  LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
  ESP_LOGCONFIG(TAG, "  Pulse time: %" PRIu32 " us", this->pulse_time_us_);
  LOG_UPDATE_INTERVAL(this);
}

float UltrasonicSensorComponent::us_to_m(uint32_t us) {
  const float speed_sound_m_per_s = 343.0f;
  const float time_s = us / 1e6f;
  const float total_dist = time_s * speed_sound_m_per_s;
  return total_dist / 2.0f;
}

}  // namespace esphome::ultrasonic
