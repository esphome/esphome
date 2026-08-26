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
// Margin past a transmission's expected duration before the chain is declared stalled
static constexpr uint32_t STALL_MARGIN_MS = 1000;
// Longest single one-shot armed; longer durations are chained (ROM us->tick headroom unverified)
static constexpr uint32_t MAX_ONE_SHOT_US = 50000;

// Shared envelope timer: a second gtimer_init on the same id fails silently, so all
// instances serialize on s_active_transmitter
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static uint8_t s_pwm_tick_sources[] = {GTimer1, GTimer2, GTimer3, GTimer4, GTimer5, GTimer6, 0xff};
static gtimer_t s_envelope_timer;
static bool s_envelope_timer_ready = false;
static RemoteTransmitterComponent *volatile s_active_transmitter = nullptr;
// Deadline for the in-flight transmission (millis-based); only touched from the main task
static uint32_t s_expected_end_ms = 0;
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
// Arms the shared envelope timer, chaining durations longer than MAX_ONE_SHOT_US. ISR-safe.
void IRAM_ATTR RemoteTransmitterComponent::arm_envelope_timer_(uint32_t duration_us) {
  // clamp to 1us (a zero-length one-shot never fires); the remainder must not underflow
  const uint32_t chunk = std::max(uint32_t(1), std::min(duration_us, MAX_ONE_SHOT_US));
  this->isr_wait_remaining_ = duration_us > chunk ? duration_us - chunk : 0;
  gtimer_start_one_shout(&s_envelope_timer, chunk, (void *) envelope_timer_isr, (uint32_t) this);
}

// Aborts a chain that stopped advancing: stop the timer, idle the pin, release the token.
// Every step is a no-op if the chain completed meanwhile. Task context only.
void RemoteTransmitterComponent::abort_stalled_chain_() {
  // cleared first so a straggler one-shot bails at the ISR entry check
  this->transmitting_ = false;
  gtimer_stop(&s_envelope_timer);
  pwmout_write(static_cast<pwmout_t *>(this->pwm_), this->isr_space_duty_);
  s_active_transmitter = nullptr;
  this->stall_aborted_ = true;
  this->status_set_warning("envelope timer stalled");
  ESP_LOGE(TAG, "Envelope timer stalled; transmission aborted");
  delay(1);  // let any already-latched interrupt land while the chain state is safe
}

// Delivers one deferred completion with its status bookkeeping
void RemoteTransmitterComponent::deliver_completion_() {
  if (!this->stall_aborted_)
    this->status_clear_warning();
  this->complete_pending_ = false;
  this->complete_trigger_.trigger();
}

// Writes the duty for one envelope item and arms the timer for its duration.
// Runs in ISR context (and once from send_internal to kick the chain): no logging, no allocation.
void IRAM_ATTR RemoteTransmitterComponent::start_isr_item_(size_t index) {
  const int32_t item = this->isr_data_[index];
  pwmout_write(static_cast<pwmout_t *>(this->pwm_), item > 0 ? this->isr_mark_duty_ : this->isr_space_duty_);
  this->arm_envelope_timer_(uint32_t(item > 0 ? item : -item));
}

void IRAM_ATTR RemoteTransmitterComponent::advance_envelope_isr() {
  if (!this->transmitting_)
    return;  // chain was aborted; this is a stale one-shot that was already latched
  if (this->isr_wait_remaining_ > 0) {
    // continue a duration longer than one hardware one-shot
    this->arm_envelope_timer_(this->isr_wait_remaining_);
    return;
  }
  if (this->isr_in_gap_) {
    // inter-repeat gap elapsed; restart the item chain
    this->isr_in_gap_ = false;
    this->isr_index_ = 0;
    this->start_isr_item_(0);
    return;
  }
  this->isr_index_++;
  if (this->isr_index_ < this->isr_data_.size()) {
    this->start_isr_item_(this->isr_index_);
    return;
  }
  // end of one repetition
  pwmout_write(static_cast<pwmout_t *>(this->pwm_), this->isr_space_duty_);
  if (this->isr_repeats_left_ > 1) {
    this->isr_repeats_left_--;
    this->isr_index_ = 0;
    if (this->isr_send_wait_ > 0) {
      this->isr_in_gap_ = true;
      this->arm_envelope_timer_(this->isr_send_wait_);
    } else {
      this->start_isr_item_(0);
    }
    return;
  }
  this->transmitting_ = false;
  s_active_transmitter = nullptr;
}

// Waits until no chain is in flight, delivering any deferred completions; a completion
// automation may start a new send, so repeat until truly idle. Bounded by the stall deadline.
void RemoteTransmitterComponent::wait_until_idle_() {
  while (true) {
    while (true) {
      // snapshot: the final ISR can clear the volatile pointer between a check and a use
      auto *active = s_active_transmitter;
      if (active == nullptr)
        break;
      if ((int32_t) (millis() - s_expected_end_ms) > 0) {
        active->abort_stalled_chain_();
        break;
      }
      App.feed_wdt();
      delay(1);
    }
    if (!this->complete_pending_)
      break;
    this->deliver_completion_();
  }
}

// Retunes the PWM period when the carrier changes; the ISR sets duty per item
void RemoteTransmitterComponent::update_carrier_(uint32_t carrier_frequency) {
  if (carrier_frequency == 0 || carrier_frequency == this->current_carrier_frequency_)
    return;
  // round(1000000/freq), clamped so a bad lambda can't hand the SDK a zero period
  const uint32_t period = std::max(uint32_t(1), (1000000UL + carrier_frequency / 2) / carrier_frequency);
  pwmout_period_us(static_cast<pwmout_t *>(this->pwm_), period);
  this->current_carrier_frequency_ = carrier_frequency;
}

// Stages the repeat schedule and stall deadline, then starts the interrupt chain
void RemoteTransmitterComponent::arm_chain_(uint32_t send_times, uint32_t send_wait) {
  this->isr_repeats_left_ = send_times;
  this->isr_send_wait_ = send_wait;
  this->isr_index_ = 0;
  this->isr_in_gap_ = false;
  this->stall_aborted_ = false;
  uint64_t frame_us = 0;
  for (int32_t item : this->isr_data_)
    frame_us += uint32_t(item > 0 ? item : -item);
  const uint64_t total_us = frame_us * send_times + uint64_t(send_wait) * (send_times - 1);
  s_expected_end_ms = millis() + uint32_t(total_us / 1000) + STALL_MARGIN_MS;
  this->transmitting_ = true;
  s_active_transmitter = this;
  this->start_isr_item_(0);
}

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  if (this->pwm_ == nullptr) {
    ESP_LOGW(TAG, "Cannot send: PWM not initialized");
    return;
  }
  this->wait_until_idle_();
  if (send_times == 0) {
    // parity with the loop-based implementations: transmit nothing, but both triggers
    // still fire so an on_complete-sequenced automation does not stall
    this->transmit_trigger_.trigger();
    this->deliver_completion_();
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
  this->update_carrier_(carrier_frequency);
  // own copy: with non_blocking the caller may re-encode temp_ while this frame is in flight
  this->isr_data_.assign(this->temp_.get_data().begin(), this->temp_.get_data().end());
  if (this->isr_data_.empty()) {
    ESP_LOGW(TAG, "Empty data");
    this->transmit_trigger_.trigger();
    this->deliver_completion_();
    return;
  }
  this->isr_mark_duty_ = mark_duty;
  this->isr_space_duty_ = space_duty;
  // trigger first: the deadline computed in arm_chain_ must not be charged for user code
  this->transmit_trigger_.trigger();
  // the automation may have started a send on another instance; let it finish before
  // claiming the shared timer (a same-instance send remains unsupported here)
  this->wait_until_idle_();
  this->arm_chain_(send_times, send_wait);
  if (this->non_blocking_) {
    this->complete_pending_ = true;
    this->enable_loop();
    return;
  }
  // blocking mode: wait out the chain, bounded by the stall deadline
  while (this->transmitting_) {
    if ((int32_t) (millis() - s_expected_end_ms) > 0) {
      this->abort_stalled_chain_();
      break;
    }
    App.feed_wdt();
    delay(1);
  }
  this->deliver_completion_();
}

void RemoteTransmitterComponent::loop() {
  if (!this->complete_pending_) {
    this->disable_loop();
    return;
  }
  if (this->transmitting_) {
    // non-blocking stall recovery: without this, a dead chain would leave the carrier
    // driven and on_complete unfired until the next send happened to abort it
    if ((int32_t) (millis() - s_expected_end_ms) <= 0)
      return;
    this->abort_stalled_chain_();
  }
  // release the loop before user code runs: the automation may start a new non-blocking
  // send, and its enable_loop() must be the last writer or its completion would strand
  this->disable_loop();
  this->deliver_completion_();
}

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
