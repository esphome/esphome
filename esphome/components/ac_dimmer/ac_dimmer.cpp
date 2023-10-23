#ifdef USE_ARDUINO

#include "ac_dimmer.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>

#ifdef USE_ESP8266
#include <core_esp8266_waveform.h>
#endif
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <esp32-hal-timer.h>
#endif

namespace esphome {
namespace ac_dimmer {

static const char *const TAG = "ac_dimmer";

// Global array to store dimmer objects
static AcDimmerDataStore *all_dimmers[32];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/// Time in microseconds the gate should be held high
/// 10µs should be long enough for most triacs
/// For reference: BT136 datasheet says 2µs nominal (page 7)
/// However other factors like gate driver propagation time
/// are also considered and a really low value is not important
/// See also: https://github.com/esphome/issues/issues/1632
static const uint32_t GATE_ENABLE_TIME = 50;

/// Function called from timer interrupt
/// Input is current time in microseconds (micros())
/// Returns when next "event" is expected in µs, or 0 if no such event known.
uint32_t IRAM_ATTR HOT AcDimmerDataStore::timer_intr(uint32_t now) {
  // If no ZC signal received yet.
  if (this->crossed_zero_at == 0)
    return 0;

  uint32_t time_since_zc = now - this->crossed_zero_at;
  if (this->value == 65535 || this->value == 0) {
    return 0;
  }

  if (this->enable_time_us != 0 && time_since_zc >= this->enable_time_us) {
    this->enable_time_us = 0;
    this->gate_pin.digital_write(true);
    // Prevent too short pulses
    this->disable_time_us = std::max(this->disable_time_us, time_since_zc + GATE_ENABLE_TIME);
  }
  if (this->disable_time_us != 0 && time_since_zc >= this->disable_time_us) {
    this->disable_time_us = 0;
    this->gate_pin.digital_write(false);
  }

  if (time_since_zc < this->enable_time_us) {
    // Next event is enable, return time until that event
    return this->enable_time_us - time_since_zc;
  } else if (time_since_zc < disable_time_us) {
    // Next event is disable, return time until that event
    return this->disable_time_us - time_since_zc;
  }

  if (time_since_zc >= this->cycle_time_us) {
    // Already past last cycle time, schedule next call shortly
    return 100;
  }

  return this->cycle_time_us - time_since_zc;
}

/// Run timer interrupt code and return in how many µs the next event is expected
uint32_t IRAM_ATTR HOT timer_interrupt() {
  // run at least with 1kHz
  uint32_t min_dt_us = 1000;
  uint32_t now = micros();
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr) {
      // no more dimmers
      break;
    }
    uint32_t res = dimmer->timer_intr(now);
    if (res != 0 && res < min_dt_us)
      min_dt_us = res;
  }
  // return time until next timer1 interrupt in µs
  return min_dt_us;
}

/// GPIO interrupt routine, called when ZC pin triggers
void IRAM_ATTR HOT AcDimmerDataStore::gpio_intr() {
  uint32_t prev_crossed = this->crossed_zero_at;

  // 50Hz mains frequency should give a half cycle of 10ms a 60Hz will give 8.33ms
  // in any case the cycle last at least 5ms
  this->crossed_zero_at = micros();
  uint32_t cycle_time = this->crossed_zero_at - prev_crossed;
  if (cycle_time > 5000) {
    this->cycle_time_us = cycle_time;
  } else {
    // Otherwise this is noise and this is 2nd (or 3rd...) fall in the same pulse
    // Consider this is the right fall edge and accumulate the cycle time instead
    this->cycle_time_us += cycle_time;
  }

  if (this->value == 65535) {
    // fully on, enable output immediately
    this->gate_pin.digital_write(true);
  } else if (this->init_cycle) {
    // send a full cycle
    this->init_cycle = false;
    this->enable_time_us = 0;
    this->disable_time_us = cycle_time_us;
  } else if (this->value == 0) {
    // fully off, disable output immediately
    this->gate_pin.digital_write(false);
  } else {
    if (this->method == DIM_METHOD_TRAILING) {
      this->enable_time_us = 1;  // cannot be 0
      this->disable_time_us = std::max((uint32_t) 10, this->value * this->cycle_time_us / 65535);
    } else {
      // calculate time until enable in µs: (1.0-value)*cycle_time, but with integer arithmetic
      // also take into account min_power
      auto min_us = this->cycle_time_us * this->min_power / 1000;
      this->enable_time_us = std::max((uint32_t) 1, ((65535 - this->value) * (this->cycle_time_us - min_us)) / 65535);

      if (this->method == DIM_METHOD_LEADING_PULSE) {
        // Minimum pulse time should be enough for the triac to trigger when it is close to the ZC zone
        // this is for brightness near 99%
        this->disable_time_us = std::max(this->enable_time_us + GATE_ENABLE_TIME, (uint32_t) cycle_time_us / 10);
      } else {
        this->gate_pin.digital_write(false);
        this->disable_time_us = this->cycle_time_us;
      }
    }
  }
}

void IRAM_ATTR HOT AcDimmerDataStore::s_gpio_intr(AcDimmerDataStore *store) {
  // Attaching pin interrupts on the same pin will override the previous interrupt
  // However, the user expects that multiple dimmers sharing the same ZC pin will work.
  // We solve this in a bit of a hacky way: On each pin interrupt, we check all dimmers
  // if any of them are using the same ZC pin, and also trigger the interrupt for *them*.
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      break;
    if (dimmer->zero_cross_pin_number == store->zero_cross_pin_number) {
      dimmer->gpio_intr();
    }
  }
}

#ifdef USE_ESP32
// ESP32 implementation, uses basically the same code but needs to wrap
// timer_interrupt() function to auto-reschedule
static hw_timer_t *dimmer_timer = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
void IRAM_ATTR HOT AcDimmerDataStore::s_timer_intr() { timer_interrupt(); }
#endif

void AcDimmer::setup() {
  // extend all_dimmers array with our dimmer

  // Need to be sure the zero cross pin is setup only once, ESP8266 fails and ESP32 seems to fail silently
  auto setup_zero_cross_pin = true;

  for (auto &all_dimmer : all_dimmers) {
    if (all_dimmer == nullptr) {
      all_dimmer = &this->store_;
      break;
    }
    if (all_dimmer->zero_cross_pin_number == this->zero_cross_pin_->get_pin()) {
      setup_zero_cross_pin = false;
    }
  }

  this->gate_pin_->setup();
  this->store_.gate_pin = this->gate_pin_->to_isr();
  this->store_.zero_cross_pin_number = this->zero_cross_pin_->get_pin();
  this->store_.min_power = static_cast<uint16_t>(this->min_power_ * 1000);
  this->min_power_ = 0;
  this->store_.method = this->method_;

  if (setup_zero_cross_pin) {
    this->zero_cross_pin_->setup();
    this->store_.zero_cross_pin = this->zero_cross_pin_->to_isr();
    this->zero_cross_pin_->attach_interrupt(&AcDimmerDataStore::s_gpio_intr, &this->store_,
                                            gpio::INTERRUPT_FALLING_EDGE);
  }

#ifdef USE_ESP8266
  // Uses ESP8266 waveform (soft PWM) class
  // PWM and AcDimmer can even run at the same time this way
  setTimer1Callback(&timer_interrupt);
#endif
#ifdef USE_ESP32
  // 80 Divider -> 1 count=1µs
  dimmer_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(dimmer_timer, &AcDimmerDataStore::s_timer_intr, true);
  // For ESP32, we can't use dynamic interval calculation because the timerX functions
  // are not callable from ISR (placed in flash storage).
  // Here we just use an interrupt firing every 50 µs.
  timerAlarmWrite(dimmer_timer, 50, true);
  timerAlarmEnable(dimmer_timer);
#endif
}
void AcDimmer::write_state(float state) {
  state = std::acos(1 - (2 * state)) / 3.14159;  // RMS power compensation
  auto new_value = static_cast<uint16_t>(roundf(state * 65535));
  if (new_value != 0 && this->store_.value == 0)
    this->store_.init_cycle = this->init_with_half_cycle_;
  this->store_.value = new_value;
}
void AcDimmer::dump_config() {
  ESP_LOGCONFIG(TAG, "AcDimmer:");
  LOG_PIN("  Output Pin: ", this->gate_pin_);
  LOG_PIN("  Zero-Cross Pin: ", this->zero_cross_pin_);
  ESP_LOGCONFIG(TAG, "   Min Power: %.1f%%", this->store_.min_power / 10.0f);
  ESP_LOGCONFIG(TAG, "   Init with half cycle: %s", YESNO(this->init_with_half_cycle_));
  if (method_ == DIM_METHOD_LEADING_PULSE) {
    ESP_LOGCONFIG(TAG, "   Method: leading pulse");
  } else if (method_ == DIM_METHOD_LEADING) {
    ESP_LOGCONFIG(TAG, "   Method: leading");
  } else {
    ESP_LOGCONFIG(TAG, "   Method: trailing");
  }

  LOG_FLOAT_OUTPUT(this);
  ESP_LOGV(TAG, "  Estimated Frequency: %.3fHz", 1e6f / this->store_.cycle_time_us / 2);
}

}  // namespace ac_dimmer
}  // namespace esphome

#endif  // USE_ARDUINO
