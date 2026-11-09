#include "remote_transmitter.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#if (defined(USE_LIBRETINY) && !defined(USE_RTL87XX) && !defined(REMOTE_TRANSMITTER_BK_PWM)) || \
    defined(USE_ESP8266) || defined(USE_RP2) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)

namespace esphome::remote_transmitter {

ESPHOME_LOG_TAG(TAG, "remote_transmitter");
#ifdef USE_ESP8266
// Holding interrupts off about 1 ms makes the ESP8266 Wi-Fi SDK hang until a watchdog resets the chip, so a frame
// is not locked whole there: interrupts are on until this long before each edge.
static constexpr int32_t EDGE_LOCK_US = 50;
#endif

void RemoteTransmitterComponent::setup() {
  this->pin_->setup();
  this->pin_->digital_write(false);
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Remote Transmitter:\n"
                "  Carrier Duty: %u%%",
                this->carrier_duty_percent_);
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

void RemoteTransmitterComponent::await_target_time_(int32_t lead_us) {
  const uint32_t current_time = micros();
  if (this->target_time_ == 0) {
    this->target_time_ = current_time;
  } else if ((int32_t) (this->target_time_ - current_time) > lead_us) {
    // busy loop, as interrupts can be disabled here and delayMicroseconds()
    // may not work correctly in interrupt-disabled contexts on all platforms
    while ((int32_t) (this->target_time_ - micros()) > lead_us)
      ;
  }
}

void RemoteTransmitterComponent::write_at_target_(bool value) {
#ifdef USE_ESP8266
  this->await_target_time_(EDGE_LOCK_US);
  InterruptLock lock;
#endif
  this->await_target_time_();
  this->pin_->digital_write(value);
}

void RemoteTransmitterComponent::mark_(uint32_t on_time, uint32_t off_time, uint32_t usec) {
  this->write_at_target_(true);

  const uint32_t target = this->target_time_ + usec;
  if (this->carrier_duty_percent_ < 100 && (on_time > 0 || off_time > 0)) {
    while (true) {  // Modulate with carrier frequency
      // Skip whole periods an interrupt already used up, so the falling edge is never already due (a runt pulse)
      while ((int32_t) (micros() - (this->target_time_ + on_time)) >= 0)
        this->target_time_ += on_time + off_time;
      this->target_time_ += on_time;
      if ((int32_t) (this->target_time_ - target) >= 0)
        break;
      this->write_at_target_(false);

      this->target_time_ += off_time;
      if ((int32_t) (this->target_time_ - target) >= 0)
        break;
      this->write_at_target_(true);
    }
  }
  this->target_time_ = target;
}

void RemoteTransmitterComponent::space_(uint32_t usec) {
  this->write_at_target_(false);
  this->target_time_ += usec;
}

void RemoteTransmitterComponent::digital_write(bool value) { this->pin_->digital_write(value); }

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  ESP_LOGD(TAG, "Sending remote code");
  uint32_t on_time, off_time;
  this->calculate_on_off_time_(this->temp_.get_carrier_frequency(), &on_time, &off_time);
  this->transmit_trigger_.trigger();
  for (uint32_t i = 0; i < send_times; i++) {
    {
#ifndef USE_ESP8266
      InterruptLock lock;
#endif
      // Re-anchor every iteration: outside ESP8266 timing must never span a lock boundary, as micros() can
      // jump when interrupts are re-enabled between repeats (e.g. LibreTiny's Beken micros()
      // discards its interrupt-lock correction, stretching the repeat gap by the lock duration).
      // ESP8266 micros() does not jump, so there the lock is taken per edge in write_at_target_().
      this->target_time_ = 0;
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
      this->write_at_target_(false);  // wait for duration of last pulse
    }

    if (i + 1 < send_times) {
      // Wait out the repeat gap with interrupts enabled: wait_time is unbounded user config
      // (previously this spin ran inside the next iteration's lock, disabling interrupts for
      // the whole gap). Anchoring after the lock release keeps it exact on all platforms.
      const uint32_t gap_end = micros() + send_wait;
      while ((int32_t) (gap_end - micros()) > 0) {
        App.feed_wdt();
      }
    }
  }
  this->fire_complete_(send_times != 0);  // same answer as the ISR backend for a frame sent zero times
}

}  // namespace esphome::remote_transmitter

#endif
