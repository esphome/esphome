#include "remote_transmitter.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Realtek SDK headers pulled in via ArduinoPrivate.h
#if defined(USE_RTL87XX) && !defined(CLANG_TIDY)

// ArduinoPrivate.h = Arduino.h + the SDK's mbed HAL (pwmout, gtimer) with the core's fixes for
// type-name collisions between the two (e.g. PinMode)
#include <ArduinoPrivate.h>
#ifndef USE_LIBRETINY_VARIANT_RTL8720C
#include <FreeRTOS.h>
#include <task.h>
#endif

namespace esphome::remote_transmitter {

static const char *const TAG = "remote_transmitter";

// PWM peripheral carrier, envelope paced by a gtimer interrupt chain. Bit-banging would need
// interrupts disabled for the whole frame, but this core's micros() derives from the FreeRTOS
// tick and freezes then. The SDK pwmout HAL is driven directly: the Arduino wiring layer's
// GPIO/PWM mode round-trip use-after-frees LibreTiny's per-pin state.

#ifdef USE_LIBRETINY_VARIANT_RTL8720C
static constexpr uint32_t ENVELOPE_TIMER_ID = TIMER6;  // GTimer7

// One envelope timer for all instances: a second gtimer_init on the same id fails silently,
// so the chain serializes them (remote_transmitter_libretiny_isr.cpp)
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static uint8_t s_pwm_tick_sources[] = {GTimer1, GTimer2, GTimer3, GTimer4, GTimer5, GTimer6, 0xff};
static gtimer_t s_envelope_timer;
static bool s_envelope_timer_ready = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static void IRAM_ATTR envelope_timer_isr(uint32_t arg) {
  reinterpret_cast<RemoteTransmitterComponent *>(arg)->advance_envelope_isr();
}
#endif  // USE_LIBRETINY_VARIANT_RTL8720C

void RemoteTransmitterComponent::setup() {
  // no pin_->setup(): a GPIO claim in the SDK's pin management blocks pwmout_init from
  // owning the pad
  PinInfo *info = pinInfo(this->pin_->get_pin());
  if (info == nullptr || !pinSupported(info, PIN_PWM)) {
    // checked here because the AmebaZ (RTL8710B) SDK does not report PWM init failure
    ESP_LOGE(TAG, "Pin %u is not PWM-capable", this->pin_->get_pin());
    this->mark_failed();
    return;
  }
  auto *pwm = new pwmout_t();
  this->pwm_ = pwm;
  pwmout_init(pwm, static_cast<PinName>(info->gpio));
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  // only the AmebaZ2 SDK's pwmout_s reports init success
  if (!pwm->is_init) {
    ESP_LOGE(TAG, "PWM init failed on pin %u", this->pin_->get_pin());
    delete pwm;
    this->pwm_ = nullptr;
    this->mark_failed();
    return;
  }
  // Shrink the PWM tick-source pool before the period claim below so GTimer7 stays free
  // for the envelope; pwmout_init just registered the full pool.
  hal_pwm_comm_tick_source_list(s_pwm_tick_sources);
#endif
  pwmout_period_us(pwm, 26);  // placeholder; the real carrier period is set per transmission
  pwmout_write(pwm, this->pin_->is_inverted() ? 1.0f : 0.0f);
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  if (!s_envelope_timer_ready) {
    gtimer_init(&s_envelope_timer, ENVELOPE_TIMER_ID);
    s_envelope_timer_ready = true;
  }
  this->disable_loop();  // loop() is only needed while a non-blocking completion is pending
#endif
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Remote Transmitter:\n"
                "  Carrier Duty: %u%%",
                this->carrier_duty_percent_);
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  ESP_LOGCONFIG(TAG, "  Non-blocking: %s", YESNO(this->non_blocking_));
#endif
  LOG_PIN("  Pin: ", this->pin_);
}

void RemoteTransmitterComponent::digital_write(bool value) {
  if (this->pwm_ == nullptr)
    return;
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  // serialize behind an in-flight chain, matching the ESP32/RMT non-blocking behavior
  this->wait_until_idle_();
#endif
  pwmout_write(static_cast<pwmout_t *>(this->pwm_), (value != this->pin_->is_inverted()) ? 1.0f : 0.0f);
}

#ifdef USE_LIBRETINY_VARIANT_RTL8720C
// --- envelope chain hooks (see remote_transmitter_libretiny_isr.cpp) ---

bool RemoteTransmitterComponent::envelope_ready_() const { return this->pwm_ != nullptr; }

// Retunes the PWM period when the carrier changes and stages the per-item duties;
// unmodulated protocols (no carrier or 100% duty) drive the pin constantly during marks
void RemoteTransmitterComponent::prepare_carrier_(uint32_t carrier_frequency) {
  float mark_duty =
      (carrier_frequency > 0 && this->carrier_duty_percent_ < 100) ? this->carrier_duty_percent_ / 100.0f : 1.0f;
  float space_duty = 0.0f;
  if (this->pin_->is_inverted()) {
    mark_duty = 1.0f - mark_duty;
    space_duty = 1.0f;
  }
  this->isr_mark_duty_ = mark_duty;
  this->isr_space_duty_ = space_duty;
  if (carrier_frequency == 0 || carrier_frequency == this->current_carrier_frequency_)
    return;
  // round(1000000/freq), clamped so a bad lambda can't hand the SDK a zero period
  const uint32_t period = std::max(uint32_t(1), (1000000UL + carrier_frequency / 2) / carrier_frequency);
  pwmout_period_us(static_cast<pwmout_t *>(this->pwm_), period);
  this->current_carrier_frequency_ = carrier_frequency;
}

void IRAM_ATTR RemoteTransmitterComponent::write_envelope_level_(bool mark) {
  pwmout_write(static_cast<pwmout_t *>(this->pwm_), mark ? this->isr_mark_duty_ : this->isr_space_duty_);
}

void IRAM_ATTR RemoteTransmitterComponent::arm_one_shot_(uint32_t duration_us) {
  gtimer_start_one_shout(&s_envelope_timer, duration_us, (void *) envelope_timer_isr, (uint32_t) this);
}

void IRAM_ATTR RemoteTransmitterComponent::stop_envelope_timer_() { gtimer_stop(&s_envelope_timer); }

#else  // !USE_LIBRETINY_VARIANT_RTL8720C -- AmebaZ (RTL8710B): spin-based envelope, per-frame priority boost

void RemoteTransmitterComponent::await_target_time_() {
  const uint32_t current_time = micros();
  if (this->target_time_ == 0) {
    this->target_time_ = current_time;
  } else {
    while ((int32_t) (this->target_time_ - micros()) > 0) {
    }
  }
}

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  if (this->pwm_ == nullptr) {
    ESP_LOGW(TAG, "Cannot send: PWM not initialized");
    return;
  }
  ESP_LOGD(TAG, "Sending remote code");
  const uint32_t carrier_frequency = this->temp_.get_carrier_frequency();
  // unmodulated protocols (no carrier or 100% duty) drive the pin constantly during marks
  float mark_duty =
      (carrier_frequency > 0 && this->carrier_duty_percent_ < 100) ? this->carrier_duty_percent_ / 100.0f : 1.0f;
  float space_duty = 0.0f;
  if (this->pin_->is_inverted()) {
    mark_duty = 1.0f - mark_duty;
    space_duty = 1.0f;
  }
  auto *pwm = static_cast<pwmout_t *>(this->pwm_);
  if (carrier_frequency > 0 && carrier_frequency != this->current_carrier_frequency_) {
    // round(1000000/freq), clamped like the bit-bang path so a bad lambda can't hand the SDK a zero period
    const uint32_t period = std::max(uint32_t(1), (1000000UL + carrier_frequency / 2) / carrier_frequency);
    pwmout_period_us(pwm, period);
    this->current_carrier_frequency_ = carrier_frequency;
  }
  this->transmit_trigger_.trigger();
  const UBaseType_t saved_priority = uxTaskPriorityGet(nullptr);
  for (uint32_t i = 0; i < send_times; i++) {
    // Boost task priority for the frame only, so WiFi/lwIP tasks can't preempt mid-frame and
    // merge adjacent marks. Interrupts stay enabled: micros() needs the FreeRTOS tick, and
    // ISR latency is within receiver tolerance.
    vTaskPrioritySet(nullptr, configMAX_PRIORITIES - 1);
    // Re-anchor every iteration: a late exit from the normal-priority gap wait must not
    // leave the schedule behind micros(), which would compress the next frame's leading items
    this->target_time_ = 0;
    for (int32_t item : this->temp_.get_data()) {
      const bool is_mark = item > 0;
      this->await_target_time_();
      pwmout_write(pwm, is_mark ? mark_duty : space_duty);
      this->target_time_ += is_mark ? uint32_t(item) : uint32_t(-item);
      App.feed_wdt();
    }
    this->await_target_time_();  // wait for duration of last pulse
    pwmout_write(pwm, space_duty);
    vTaskPrioritySet(nullptr, saved_priority);
    if (i + 1 < send_times) {
      // The repeat gap is user-configurable and unbounded, so wait it out at normal
      // priority, feeding the watchdog
      const uint32_t gap_end = micros() + send_wait;
      while ((int32_t) (gap_end - micros()) > 0) {
        App.feed_wdt();
      }
    }
  }
  this->complete_trigger_.trigger();
}

#endif  // USE_LIBRETINY_VARIANT_RTL8720C

}  // namespace esphome::remote_transmitter

#endif  // USE_RTL87XX && !CLANG_TIDY
