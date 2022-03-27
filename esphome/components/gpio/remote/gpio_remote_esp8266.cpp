#include "gpio_remote.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP8266

namespace esphome {
namespace gpio {

static const char *const TAG = "remote.gpio";

void GPIORemote::setup() {
  this->transmit_pin_->setup();
  this->transmit_pin_->digital_write(false);
}

void GPIORemote::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO Remote:");
  ESP_LOGCONFIG(TAG, "  Carrier Duty: %u%%", this->carrier_duty_percent_);
  LOG_PIN("  Transmit Pin: ", this->transmit_pin_);
  ESP_LOGCONFIG(TAG, "  Supported Protocols:");
  for (auto p : this->capabilities_.supports_transmit) {
    ESP_LOGCONFIG(TAG, "  - %s", p.c_str());
  }
}

// Transmitter members

void GPIORemote::calculate_on_off_time_(uint32_t carrier_frequency, uint32_t *on_time_period,
                                        uint32_t *off_time_period) {
  if (carrier_frequency == 0) {
    *on_time_period = 0;
    *off_time_period = 0;
    return;
  }
  uint32_t period = (1000000UL + carrier_frequency / 2) / carrier_frequency;  // round(1000000/freq)
  period = std::max(uint32_t(1), period);
  *on_time_period = (period * this->carrier_duty_percent_) / 100;
  *off_time_period = period - *on_time_period;
}

void GPIORemote::await_target_time_() {
  const uint32_t current_time = micros();
  if (this->target_time_ == 0) {
    this->target_time_ = current_time;
  } else if (this->target_time_ > current_time) {
    delayMicroseconds(this->target_time_ - current_time);
  }
}

void GPIORemote::mark_(uint32_t on_time, uint32_t off_time, uint32_t usec) {
  this->await_target_time_();
  this->transmit_pin_->digital_write(true);

  const uint32_t target = this->target_time_ + usec;
  if (this->carrier_duty_percent_ < 100 && (on_time > 0 || off_time > 0)) {
    while (true) {  // Modulate with carrier frequency
      this->target_time_ += on_time;
      if (this->target_time_ >= target)
        break;
      this->await_target_time_();
      this->transmit_pin_->digital_write(false);

      this->target_time_ += off_time;
      if (this->target_time_ >= target)
        break;
      this->await_target_time_();
      this->transmit_pin_->digital_write(true);
    }
  }
  this->target_time_ = target;
}

void GPIORemote::space_(uint32_t usec) {
  this->await_target_time_();
  this->transmit_pin_->digital_write(false);
  this->target_time_ += usec;
}

void GPIORemote::send_internal_(uint32_t send_times, uint32_t send_wait) {
  uint32_t on_time, off_time;
  this->calculate_on_off_time_(this->temp_.get_carrier_frequency(), &on_time, &off_time);
  this->target_time_ = 0;
  for (uint32_t i = 0; i < send_times; i++) {
    for (int32_t item : this->temp_.get_data()) {
      if (item > 0) {
        const auto length = uint32_t(item);
        this->mark_(on_time, off_time, length);
      } else {
        const auto length = uint32_t(-item);
        this->space_(length);
      }
      App.feed_wdt();
    }
    this->await_target_time_();  // wait for duration of last pulse
    this->transmit_pin_->digital_write(false);

    if (i + 1 < send_times)
      this->target_time_ += send_wait;
  }
}

}  // namespace gpio
}  // namespace esphome

#endif  // USE_ESP8266
