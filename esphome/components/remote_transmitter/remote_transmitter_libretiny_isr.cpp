#include "remote_transmitter.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

// Envelope chain shared by the LibreTiny families that pace transmission from a hardware
// timer interrupt: RTL8720C (gtimer) and the BK7231N-style PWM block (BKTIMER1). Everything
// platform-specific sits behind five hooks implemented in the per-family files -- carrier
// setup, duty writes, one-shot arming and timer stop. Families without a usable timer keep
// the generic bit-bang implementation and compile none of this.
#if defined(USE_LIBRETINY_VARIANT_RTL8720C) || defined(REMOTE_TRANSMITTER_BK_PWM)

namespace esphome::remote_transmitter {

static const char *const TAG = "remote_transmitter";

// Margin past a transmission's expected duration before the chain is declared stalled
static constexpr uint32_t STALL_MARGIN_MS = 1000;
// Longest single one-shot armed; longer durations are chained. Both families need the cap:
// the Beken driver computes period_us * 26 in 32 bits (overflows past ~165s) and the Realtek
// us->tick conversion lives in mask ROM with unverified headroom.
static constexpr uint32_t MAX_ONE_SHOT_US = 50000;

// One hardware timer is shared by all instances (MULTI_CONF), so they serialize on this
// token; the deadline always describes whichever chain currently owns it.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static RemoteTransmitterComponent *volatile s_active_transmitter = nullptr;
static uint32_t s_expected_end_ms = 0;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// Entry point for trampolines whose SDK callback carries no user argument
void IRAM_ATTR RemoteTransmitterComponent::advance_active_isr() {
  auto *transmitter = s_active_transmitter;
  if (transmitter != nullptr)
    transmitter->advance_envelope_isr();
}

// Arms the envelope timer, chaining durations longer than MAX_ONE_SHOT_US. ISR-safe.
void IRAM_ATTR RemoteTransmitterComponent::arm_envelope_timer_(uint32_t duration_us) {
  // clamp to 1us (a zero-length one-shot never fires); the remainder must not underflow
  const uint32_t chunk = std::max(uint32_t(1), std::min(duration_us, MAX_ONE_SHOT_US));
  this->isr_wait_remaining_ = duration_us > chunk ? duration_us - chunk : 0;
  this->arm_one_shot_(chunk);
}

// Writes the level for one envelope item and arms the timer for its duration.
// Runs in ISR context (and once from arm_chain_ to kick the chain): no logging, no allocation.
void IRAM_ATTR RemoteTransmitterComponent::start_isr_item_(size_t index) {
  const int32_t item = this->isr_data_[index];
  this->write_envelope_level_(item > 0);
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
  this->isr_index_ = this->isr_index_ + 1;
  if (this->isr_index_ < this->isr_data_.size()) {
    this->start_isr_item_(this->isr_index_);
    return;
  }
  // end of one repetition
  this->write_envelope_level_(false);
  if (this->isr_repeats_left_ > 1) {
    this->isr_repeats_left_ = this->isr_repeats_left_ - 1;
    this->isr_index_ = 0;
    if (this->isr_send_wait_ > 0) {
      this->isr_in_gap_ = true;
      this->arm_envelope_timer_(this->isr_send_wait_);
    } else {
      this->start_isr_item_(0);
    }
    return;
  }
  // required on Beken (its timer reloads); on Realtek this only clears the enable bit of a
  // one-shot that has already fired
  this->stop_envelope_timer_();
  this->transmitting_ = false;
  s_active_transmitter = nullptr;
}

// Aborts a chain that stopped advancing: stop the timer, idle the pin, release the token.
// Every step is a no-op if the chain completed meanwhile. Task context only.
void RemoteTransmitterComponent::abort_stalled_chain_() {
  // cleared first so a straggler one-shot bails at the ISR entry check
  this->transmitting_ = false;
  this->stop_envelope_timer_();
  this->write_envelope_level_(false);
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
  if (!this->envelope_ready_()) {
    // both triggers still fire, so an on_complete-sequenced automation does not stall
    ESP_LOGW(TAG, "Cannot send: PWM not initialized");
    this->transmit_trigger_.trigger();
    this->deliver_completion_();
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
  this->prepare_carrier_(this->temp_.get_carrier_frequency());
  // own copy: with non_blocking the caller may re-encode temp_ while this frame is in flight
  this->isr_data_.assign(this->temp_.get_data().begin(), this->temp_.get_data().end());
  if (this->isr_data_.empty()) {
    ESP_LOGW(TAG, "Empty data");
    this->transmit_trigger_.trigger();
    this->deliver_completion_();
    return;
  }
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

}  // namespace esphome::remote_transmitter

#endif  // USE_LIBRETINY_VARIANT_RTL8720C || REMOTE_TRANSMITTER_BK_PWM
