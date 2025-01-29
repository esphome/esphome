#include "remote_transmitter.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_LIBRETINY

namespace esphome {
namespace remote_transmitter {

static const char *const TAG = "remote_transmitter";

void RemoteTransmitterComponent::setup() {
  this->pin_->setup();
  this->pin_->digital_write(false);
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Transmitter...");
  ESP_LOGCONFIG(TAG, "  Carrier Duty: %u%%", this->carrier_duty_percent_);
  LOG_PIN("  Pin: ", this->pin_);
}

void RemoteTransmitterComponent::calculate_on_off_time_(uint32_t carrier_frequency, uint32_t *on_time_period,
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

void RemoteTransmitterComponent::await_target_time_() {
  const uint32_t current_time = micros();
  if (this->target_time_ == 0) {
    this->target_time_ = current_time;
  } else {
    while ((int32_t) (this->target_time_ - micros()) > 0) {
      // busy loop that ensures micros is constantly called
    }
  }
}

void RemoteTransmitterComponent::mark_(uint32_t on_time, uint32_t off_time, uint32_t usec) {
  this->await_target_time_();
  this->pin_->digital_write(true);

  const uint32_t target = this->target_time_ + usec;
  if (this->carrier_duty_percent_ < 100 && (on_time > 0 || off_time > 0)) {
    while (true) {  // Modulate with carrier frequency
      this->target_time_ += on_time;
      if ((int32_t) (this->target_time_ - target) >= 0)
        break;
      this->await_target_time_();
      this->pin_->digital_write(false);

      this->target_time_ += off_time;
      if ((int32_t) (this->target_time_ - target) >= 0)
        break;
      this->await_target_time_();
      this->pin_->digital_write(true);
    }
  }
  this->target_time_ = target;
}

void RemoteTransmitterComponent::space_(uint32_t usec) {
  this->await_target_time_();
  this->pin_->digital_write(false);
  this->target_time_ += usec;
}

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  ESP_LOGD(TAG, "Sending remote code...");
  uint32_t on_time, off_time;
  this->calculate_on_off_time_(this->temp_.get_carrier_frequency(), &on_time, &off_time);
  this->target_time_ = 0;
  this->transmit_trigger_->trigger();
  for (uint32_t i = 0; i < send_times; i++) {
    InterruptLock lock;
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
    this->pin_->digital_write(false);

    if (i + 1 < send_times)
      this->target_time_ += send_wait;
  }
  this->complete_trigger_->trigger();
}

}  // namespace remote_transmitter
}  // namespace esphome

#endif
