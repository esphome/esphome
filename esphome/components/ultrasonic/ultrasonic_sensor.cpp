#include "ultrasonic_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ultrasonic {

static const char *const TAG = "ultrasonic.sensor";

void IRAM_ATTR UltrasonicSensorStore::gpio_intr(UltrasonicSensorStore *arg) {
  uint32_t now = micros();
  if (arg->echo_pin.digital_read()) {
    arg->echo_start_us = now;
  } else {
    arg->echo_end_us = now;
    arg->measurement_complete = true;
  }
}

void UltrasonicSensorComponent::setup() {
  this->trigger_pin_->setup();
  this->trigger_pin_->digital_write(false);
  this->echo_pin_->setup();
  this->store_.echo_pin = this->echo_pin_->to_isr();
  this->echo_pin_->attach_interrupt(UltrasonicSensorStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}

void UltrasonicSensorComponent::update() {
  if (this->measurement_pending_) {
    return;
  }

  this->store_.echo_start_us = 0;
  this->store_.echo_end_us = 0;
  this->store_.measurement_complete = false;
  this->measurement_pending_ = true;
  this->measurement_start_us_ = micros();

  InterruptLock lock;
  this->trigger_pin_->digital_write(true);
  delayMicroseconds(this->pulse_time_us_);
  this->trigger_pin_->digital_write(false);
}

void UltrasonicSensorComponent::loop() {
  if (!this->measurement_pending_) {
    return;
  }

  if (this->store_.measurement_complete) {
    uint32_t pulse_duration = this->store_.echo_end_us - this->store_.echo_start_us;
    ESP_LOGV(TAG, "Echo took %" PRIu32 "us", pulse_duration);
    float result = UltrasonicSensorComponent::us_to_m(pulse_duration);
    ESP_LOGD(TAG, "'%s' - Got distance: %.3f m", this->name_.c_str(), result);
    this->publish_state(result);
    this->measurement_pending_ = false;
    return;
  }

  if ((micros() - this->measurement_start_us_) >= this->timeout_us_) {
    ESP_LOGD(TAG, "'%s' - Distance measurement timed out!", this->name_.c_str());
    this->publish_state(NAN);
    this->measurement_pending_ = false;
  }
}

void UltrasonicSensorComponent::dump_config() {
  LOG_SENSOR("", "Ultrasonic Sensor", this);
  LOG_PIN("  Echo Pin: ", this->echo_pin_);
  LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
  ESP_LOGCONFIG(TAG,
                "  Pulse time: %" PRIu32 " us\n"
                "  Timeout: %" PRIu32 " us",
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
