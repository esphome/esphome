#include "remote_transmitter.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Beken SDK headers pulled in via ArduinoPrivate.h
#if defined(USE_BK72XX) && !defined(CLANG_TIDY)

// ArduinoPrivate.h = Arduino.h + the BDK SDK headers (pwm_pub.h, bk_timer_pub.h, icu_pub.h)
// with the core's fixes for type-name collisions between the two
#include <ArduinoPrivate.h>

// Only the BK7231N-style PWM block (shadow registers with a hardware CFG_UPDATA load bit)
// supports glitch-free per-edge duty updates; older SoCs compile the generic bit-bang
// implementation (remote_transmitter.cpp) instead, and this file compiles to nothing.
// REMOTE_TRANSMITTER_BK_PWM is set per-family in remote_transmitter.h.

namespace esphome::remote_transmitter {

static const char *const TAG = "remote_transmitter";

#ifdef REMOTE_TRANSMITTER_BK_PWM

// PWM peripheral carrier (26MHz block), envelope paced by a BKTIMER1 interrupt chain: each
// interrupt writes the next duty through the shadow registers (T1..T4 + CFG_UPDATA hardware
// load, glitch-free at the next carrier period). Direct register writes beat the driver's
// pwm_update_param() (~19us vs ~26us edge error) and have no shared state to race against.
// BKTIMER1 is the only free channel: TIMER0 = FreeRTOS tick, TIMER2 = SDK cal, TIMER4 = wdt.

static constexpr uint32_t REG_PWM_BASE = 0x00802B00UL;
static constexpr uint32_t REG_PWM_GROUP_STRIDE = 0x40;       // one register group per channel pair
static constexpr uint32_t REG_PWM_T_REGS[2] = {0x04, 0x14};  // T1..T4 offsets within a group
static constexpr uint32_t PWM_INT_STATUS_MASK = 3UL << 30;   // write-1-clear -- always write as zero
static constexpr uint8_t ENVELOPE_TIMER = BKTIMER1;
// Margin past a transmission's expected duration before the chain is declared stalled
static constexpr uint32_t STALL_MARGIN_MS = 1000;
// Longest single one-shot armed; longer durations are chained (driver computes period_us * 26
// in 32 bits, overflowing past ~165s)
static constexpr uint32_t MAX_ONE_SHOT_US = 50000;

// The bk_timer handler receives only the channel number, so the active instance is tracked
// statically; it doubles as the cross-instance serialization token (MULTI_CONF).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static RemoteTransmitterComponent *volatile s_active_transmitter = nullptr;
// Deadline for the in-flight transmission (millis-based); only touched from the main task
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static uint32_t s_expected_end_ms = 0;

// No IRAM_ATTR: hal.h makes it a no-op on BK72xx (the SDK masks IRQs around flash writes)
static void envelope_timer_isr(UINT8 channel) {
  auto *transmitter = s_active_transmitter;
  if (transmitter != nullptr)
    transmitter->advance_envelope_isr();
}

// PWM channel per pin on the BK7231N-style block
struct PwmPinChannel {
  uint8_t pin;
  int8_t channel;
};
static constexpr PwmPinChannel PWM_PIN_CHANNELS[] = {{6, 0}, {7, 1}, {8, 2}, {9, 3}, {24, 4}, {26, 5}};

static int8_t pwm_channel_for_pin(uint8_t pin) {
  for (const auto &entry : PWM_PIN_CHANNELS) {
    if (entry.pin == pin)
      return entry.channel;
  }
  return -1;
}

void RemoteTransmitterComponent::setup() {
  // Deliberately no pin_->setup(): the pin must belong to the PWM function, not GPIO
  const int8_t channel = pwm_channel_for_pin(this->pin_->get_pin());
  if (channel < 0) {
    ESP_LOGE(TAG, "Pin %u is not PWM-capable", this->pin_->get_pin());
    this->mark_failed();
    return;
  }
  this->pwm_channel_ = channel;
  const uint32_t idle_t1 = this->pin_->is_inverted() ? this->isr_period_t4_ : 0;
  pwm_param_st param{};
  param.chan = channel;
  param.t1 = idle_t1;
  param.t4 = this->isr_period_t4_;
  param.init_level = idle_t1 ? 1 : 0;
  if (pwm_init_param(&param) != 0 || pwm_start(channel) != 0) {
    ESP_LOGE(TAG, "PWM init failed on pin %u", this->pin_->get_pin());
    this->pwm_channel_ = -1;
    this->mark_failed();
    return;
  }
  this->disable_loop();  // loop() is only needed while a non-blocking completion is pending
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Remote Transmitter:\n"
                "  Carrier Duty: %u%%\n"
                "  Non-blocking: %s",
                this->carrier_duty_percent_, YESNO(this->non_blocking_));
  LOG_PIN("  Pin: ", this->pin_);
}

// Writes the duty compare registers and sets the hardware CFG_UPDATA shadow-load bit;
// the new duty latches glitch-free at the next carrier period. ISR-safe: registers only.
void RemoteTransmitterComponent::write_pwm_t1_(uint32_t t1_counts) {
  const uint32_t group = this->pwm_channel_ / 2;
  const uint32_t post = this->pwm_channel_ % 2;
  const uint32_t group_base = REG_PWM_BASE + REG_PWM_GROUP_STRIDE * group;
  auto *t_regs = (volatile uint32_t *) (group_base + REG_PWM_T_REGS[post]);
  auto *ctrl = (volatile uint32_t *) group_base;
  const uint32_t init_level_bit = 1UL << (8 * post + 6);  // output level while the counter is stopped
  const uint32_t cfg_updata_bit = 1UL << (8 * post + 7);  // 0->1 latches T1..T4 at the next period
  t_regs[0] = t1_counts;                                  // T1: high time
  t_regs[1] = 0;                                          // T2
  t_regs[2] = 0;                                          // T3
  t_regs[3] = this->isr_period_t4_;                       // T4: period
  uint32_t cfg = *ctrl;
  cfg &= ~(PWM_INT_STATUS_MASK | init_level_bit | cfg_updata_bit);
  if (t1_counts != 0)
    cfg |= init_level_bit;
  *ctrl = cfg;
  *ctrl = cfg | cfg_updata_bit;
}

// Arms the shared envelope timer, chaining durations longer than MAX_ONE_SHOT_US. ISR-safe.
void RemoteTransmitterComponent::arm_envelope_timer_(uint32_t duration_us) {
  // clamp to 1us (a zero-length one-shot never fires); the remainder must not underflow
  const uint32_t chunk = std::max(uint32_t(1), std::min(duration_us, MAX_ONE_SHOT_US));
  this->isr_wait_remaining_ = duration_us > chunk ? duration_us - chunk : 0;
  timer_param_t param{};
  param.channel = ENVELOPE_TIMER;
  param.div = 1;
  param.period = chunk;
  param.t_Int_Handler = envelope_timer_isr;
  sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_INIT_PARAM_US, &param);
}

// Aborts a chain that stopped advancing: stop the timer, idle the pin, release the token.
// Every step is a no-op if the chain completed meanwhile. Task context only.
void RemoteTransmitterComponent::abort_stalled_chain_() {
  // cleared first so a straggler one-shot bails at the ISR entry check
  this->transmitting_ = false;
  UINT32 channel = ENVELOPE_TIMER;
  sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_UNIT_DISABLE, &channel);
  this->write_pwm_t1_(this->isr_space_t1_);
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

void RemoteTransmitterComponent::start_isr_item_(size_t index) {
  const int32_t item = this->isr_data_[index];
  this->write_pwm_t1_(item > 0 ? this->isr_mark_t1_ : this->isr_space_t1_);
  this->arm_envelope_timer_(uint32_t(item > 0 ? item : -item));
}

void RemoteTransmitterComponent::advance_envelope_isr() {
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
  this->write_pwm_t1_(this->isr_space_t1_);
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
  UINT32 channel = ENVELOPE_TIMER;
  sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_UNIT_DISABLE, &channel);
  this->transmitting_ = false;
  s_active_transmitter = nullptr;
}

void RemoteTransmitterComponent::digital_write(bool value) {
  if (this->pwm_channel_ < 0)
    return;
  // serialize behind an in-flight chain, matching the ESP32/RMT non-blocking behavior
  this->wait_until_idle_();
  this->write_pwm_t1_((value != this->pin_->is_inverted()) ? this->isr_period_t4_ : 0);
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
  if (this->pwm_channel_ < 0) {
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
  // period in 26MHz counts; unmodulated protocols drive the pin constantly during marks
  if (carrier_frequency > 0) {
    this->isr_period_t4_ = std::max(uint32_t(2), (26000000UL + carrier_frequency / 2) / carrier_frequency);
  }
  uint32_t mark_t1 = (carrier_frequency > 0 && this->carrier_duty_percent_ < 100)
                         ? std::max(uint32_t(1), this->isr_period_t4_ * this->carrier_duty_percent_ / 100)
                         : this->isr_period_t4_;
  uint32_t space_t1 = 0;
  if (this->pin_->is_inverted()) {
    mark_t1 = this->isr_period_t4_ - mark_t1;
    space_t1 = this->isr_period_t4_;
  }
  // own copy: with non_blocking the caller may re-encode temp_ while this frame is in flight
  this->isr_data_.assign(this->temp_.get_data().begin(), this->temp_.get_data().end());
  if (this->isr_data_.empty()) {
    ESP_LOGW(TAG, "Empty data");
    this->transmit_trigger_.trigger();
    this->deliver_completion_();
    return;
  }
  this->isr_mark_t1_ = mark_t1;
  this->isr_space_t1_ = space_t1;
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

#endif  // REMOTE_TRANSMITTER_BK_PWM

}  // namespace esphome::remote_transmitter

#endif  // USE_BK72XX && !CLANG_TIDY
