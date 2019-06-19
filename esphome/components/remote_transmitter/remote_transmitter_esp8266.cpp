#include "remote_transmitter.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP8266

namespace esphome {
namespace remote_transmitter {

static const char *TAG = "remote_transmitter";

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

void RemoteTransmitterComponent::mark_(uint32_t on_time, uint32_t off_time, uint32_t usec) {
  if (this->carrier_duty_percent_ == 100 || (on_time == 0 && off_time == 0)) {
    this->pin_->digital_write(true);
    delay_microseconds_accurate(usec);
    this->pin_->digital_write(false);
    return;
  }

  const uint32_t start_time = micros();
  uint32_t current_time = start_time;

  while (current_time - start_time < usec) {
    const uint32_t elapsed = current_time - start_time;
    this->pin_->digital_write(true);

    delay_microseconds_accurate(std::min(on_time, usec - elapsed));
    this->pin_->digital_write(false);
    if (elapsed + on_time >= usec)
      return;

    delay_microseconds_accurate(std::min(usec - elapsed - on_time, off_time));

    current_time = micros();
  }
}
void RemoteTransmitterComponent::space_(uint32_t usec) {
  this->pin_->digital_write(false);
  delay_microseconds_accurate(usec);
}
void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  ESP_LOGD(TAG, "Sending remote code...");
  uint32_t on_time, off_time;
  this->calculate_on_off_time_(this->temp_.get_carrier_frequency(), &on_time, &off_time);
  for (uint32_t i = 0; i < send_times; i++) {
    disable_interrupts();
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
    enable_interrupts();

    if (i + 1 < send_times) {
      delay(send_wait / 1000UL);
      delayMicroseconds(send_wait % 1000UL);
    }
  }
}

}  // namespace remote_transmitter
}  // namespace esphome

#endif
