#include "dimmer.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP8266
#include <core_esp8266_waveform.h>
#endif

namespace esphome {
namespace dimmer {

static const char *TAG = "dimmer";

// Global array to store dimmer objects
static DimmerDataStore *all_dimmers[32];

/// Time in microseconds the gate should be held high
/// 10µs should be long enough for most triacs
/// For reference: BT136 datasheet says 2µs nominal (page 7)
static uint32_t GATE_ENABLE_TIME = 10;

/// Function called from timer interrupt
/// Input is current time in microseconds (micros())
/// Returns when next "event" is expected in µs, or 0 if no such event known.
uint32_t ICACHE_RAM_ATTR HOT DimmerDataStore::timer_intr(uint32_t now) {
  // If no ZC signal received yet.
  if (this->crossed_zero_at == 0)
    return 0;

  uint32_t time_since_zc = now - this->crossed_zero_at;
  if (this->enable_time_us != 0) {
    if (time_since_zc >= this->enable_time_us) {
      this->gate_pin->digital_write(true);
      // Reset enable time
      this->enable_time_us = 0;

      // Return time until disable
      if (now > this->disable_time_us)
        // Range check
        return 1;
      return this->disable_time_us - now;
    } else {
      // Next event is enable, return time until that event
      return this->enable_time_us - time_since_zc;
    }
  }
  if (this->disable_time_us != 0) {
    if (time_since_zc >= this->disable_time_us) {
      this->gate_pin->digital_write(false);
      this->disable_time_us = 0;
      // Return run next cycle, minus

      return this->cycle_time_us - GATE_ENABLE_TIME;
    } else {
      // Next event is disable, return time until that event
      return this->disable_time_us - time_since_zc;
    }
  }

  if (time_since_zc >= this->cycle_time_us) {
    // Already past last cycle time, schedule next call shortly
    return 100;
  }
  return this->cycle_time_us - time_since_zc;
}
/// Run timer interrupt code and return in how many µs the next event is expected
uint32_t ICACHE_RAM_ATTR HOT timer_interrupt() {
  // run at least with 1kHz
  uint32_t min_dt_us = 1000;
  uint32_t now = micros();
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      // no more dimmers
      break;
    uint32_t res = dimmer->timer_intr(now);
    if (res != 0 && res < min_dt_us)
      min_dt_us = res;
  }
  // return time until next timer1 interrupt in µs
  return min_dt_us;
}

/// GPIO interrupt routine, called when ZC pin triggers
void ICACHE_RAM_ATTR HOT DimmerDataStore::gpio_intr() {
  uint32_t prev_crossed = this->crossed_zero_at;
  this->crossed_zero_at = micros();
  this->cycle_time_us = this->crossed_zero_at - prev_crossed;

  this->enable_time_us = 0;
  if (this->value == 255) {
    // fully on, enable output immediately
    this->gate_pin->digital_write(true);
  } else if (this->value == 0) {
    // fully off, disable output immediately
    this->gate_pin->digital_write(false);
  } else {
    this->gate_pin->digital_write(false);

    // calculate time until enable in µs: (1.0-value)*cycle_time, but with integer arithmetic
    this->enable_time_us = ((255 - this->value) * this->cycle_time_us) / 255;
    // Keep signal HIGH for 10µs
    this->disable_time_us = this->enable_time_us + GATE_ENABLE_TIME;
  }
}
void ICACHE_RAM_ATTR HOT DimmerDataStore::s_gpio_intr(DimmerDataStore *store) {
  // Attaching pin interrupts on the same pin will override the previous interupt
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

#ifdef ARDUINO_ARCH_ESP32
// ESP32 implementation, uses basically the same code but needs to wrap
// timer_interrupt() function to auto-reschedule
static hw_timer_t *dimmer_timer = nullptr;
void ICACHE_RAM_ATTR HOT DimmerDataStore::s_timer_intr() {
  timer_interrupt();
}
#endif

void Dimmer::setup() {
  // extend all_dimmers array with our dimmer
  for (auto &all_dimmer : all_dimmers) {
    if (all_dimmer == nullptr) {
      all_dimmer = &this->store_;
      break;
    }
  }
  this->gate_pin_->setup();
  this->zero_cross_pin_->setup();

  this->store_.gate_pin = this->gate_pin_->to_isr();
  this->store_.zero_cross_pin = this->zero_cross_pin_->to_isr();
  this->store_.zero_cross_pin_number = this->zero_cross_pin_->get_pin();
  // TODO: why FALLING here?
  this->zero_cross_pin_->attach_interrupt(&DimmerDataStore::s_gpio_intr, &this->store_, FALLING);

#ifdef ARDUINO_ARCH_ESP8266
  // Uses ESP8266 waveform (soft PWM) class
  // PWM and Dimmer can even run at the same time this way
  setTimer1Callback(&timer_interrupt);
#endif
#ifdef ARDUINO_ARCH_ESP32
  // 80 Divider -> 1 count=1µs
  dimmer_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(dimmer_timer, &DimmerDataStore::s_timer_intr, true);
  // For ESP32, we can't use dynamic interval calculation because the timerX functions
  // are not callable from ISR (placed in flash storage).
  // Here we just use an interrupt firing every 50 µs.
  timerAlarmWrite(dimmer_timer, 50, true);
  timerAlarmEnable(dimmer_timer);
#endif
}
void Dimmer::write_state(float state) { this->store_.value = roundf(state * 255); }
void Dimmer::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP8266 PWM:");
  LOG_PIN("  Output Pin: ", this->gate_pin_);
  LOG_PIN("  Zero-Cross Pin: ", this->zero_cross_pin_);
  ESP_LOGV(TAG, "  Estimated Frequency: %.3fHz", 1e6f / this->store_.cycle_time_us);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace dimmer
}  // namespace esphome
