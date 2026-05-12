#include "ir_rf_proxy.h"

#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::ir_rf_proxy {

static const char *const TAG = "ir_rf_proxy";

// ========== Shared transmit helper ==========
// Static template: all instantiations occur in this translation unit.

template<typename CallT>
static void transmit_raw_timings(remote_base::RemoteTransmitterBase *transmitter, uint32_t carrier_frequency,
                                 const CallT &call, uint32_t *out_duration_us = nullptr) {
  if (transmitter == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGE(TAG, "No raw timings provided");
    return;
  }

  auto transmit_call = transmitter->transmit();
  auto *transmit_data = transmit_call.get_data();
  transmit_data->set_carrier_frequency(carrier_frequency);

  if (call.is_packed()) {
    transmit_data->set_data_from_packed_sint32(call.get_packed_data(), call.get_packed_length(),
                                               call.get_packed_count());
    ESP_LOGD(TAG, "Transmitting packed raw timings: count=%" PRIu16 ", repeat=%" PRIu32, call.get_packed_count(),
             call.get_repeat_count());
  } else if (call.is_base64url()) {
    if (!transmit_data->set_data_from_base64url(call.get_base64url_data())) {
      ESP_LOGE(TAG, "Invalid base64url data");
      return;
    }
    constexpr int32_t max_timing_us = 500000;
    for (int32_t timing : transmit_data->get_data()) {
      int32_t abs_timing = timing < 0 ? -timing : timing;
      if (abs_timing > max_timing_us) {
        ESP_LOGE(TAG, "Invalid timing value: %" PRId32 " µs (max %" PRId32 ")", timing, max_timing_us);
        return;
      }
    }
    ESP_LOGD(TAG, "Transmitting base64url raw timings: count=%zu, repeat=%" PRIu32, transmit_data->get_data().size(),
             call.get_repeat_count());
  } else {
    transmit_data->set_data(call.get_raw_timings());
    ESP_LOGD(TAG, "Transmitting raw timings: count=%zu, repeat=%" PRIu32, call.get_raw_timings().size(),
             call.get_repeat_count());
  }

  uint32_t repeat_count = call.get_repeat_count();
  if (repeat_count > 0) {
    transmit_call.set_send_times(repeat_count);
  }

  // Compute total transmission duration in microseconds (sum of absolute timings × repeats).
  // Used by callers that need to schedule post-transmit work (e.g. CC1101 state turnaround).
  if (out_duration_us != nullptr) {
    uint32_t total = 0;
    for (int32_t timing : transmit_data->get_data()) {
      total += timing < 0 ? static_cast<uint32_t>(-timing) : static_cast<uint32_t>(timing);
    }
    *out_duration_us = total * (repeat_count > 0 ? repeat_count : 1);
  }

  transmit_call.perform();
}

// ========== IrRfProxy (Infrared platform) ==========

#ifdef USE_IR_RF

void IrRfProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "IR Proxy '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->traits_.get_supports_transmitter()),
                YESNO(this->traits_.get_supports_receiver()));

  if (this->is_rf()) {
    ESP_LOGCONFIG(TAG, "  Hardware Type: RF (%.3f MHz)", this->frequency_khz_ / 1e3f);
  } else {
    ESP_LOGCONFIG(TAG, "  Hardware Type: Infrared");
  }
}

void IrRfProxy::control(const infrared::InfraredCall &call) {
  uint32_t carrier = call.get_carrier_frequency().value_or(0);
  transmit_raw_timings(this->transmitter_, carrier, call);
}

#endif  // USE_IR_RF

// ========== RfProxy (Radio Frequency platform) ==========

#ifdef USE_RADIO_FREQUENCY

void RfProxy::setup() {
  this->traits_.set_supports_transmitter(this->transmitter_ != nullptr);
  this->traits_.set_supports_receiver(this->receiver_ != nullptr);

  // remote_transmitter/receiver always uses OOK (on-off keying)
  this->traits_.add_supported_modulation(radio_frequency::RadioFrequencyModulation::RADIO_FREQUENCY_MODULATION_OOK);

  if (this->receiver_ != nullptr) {
    this->receiver_->register_listener(this);
  }

#ifdef USE_CC1101
  // If a CC1101 is configured as the RF front-end, drive it into RX state once
  // everything else is set up.  RX is the idle listening state; TX is a transient
  // burst that control() will switch to and back as needed.  We defer the state
  // change because CC1101::setup() also defers a GDO0 pin-mode change to the next
  // loop tick — our defer queues after CC1101's, so the state ends in the right place.
  if (this->cc1101_ != nullptr) {
    // Seed the last-known frequency from the entity's configured frequency (if any)
    // so the first matching transmit doesn't retune unnecessarily.
    this->last_cc1101_frequency_hz_ = this->traits_.get_frequency_min_hz();
    this->defer([this]() { this->cc1101_->begin_rx(); });
  }
#endif
}

#ifdef USE_CC1101
bool RfProxy::on_receive(remote_base::RemoteReceiveData data) {
  // When TX and RX share a pin, the MCU driving GDO0 during a transmission will
  // generate input edges that remote_receiver sees as "received" timings.  Drop
  // them — they're our own outgoing signal echoed back.
  if (this->cc1101_ != nullptr && this->cc1101_->is_tx_busy()) {
    return false;  // don't consume the event, but skip our own forwarding
  }
  return radio_frequency::RadioFrequency::on_receive(data);
}
#endif

void RfProxy::dump_config() {
#ifdef USE_CC1101
  const char *backend =
      this->cc1101_ != nullptr ? "remote_transmitter/receiver via CC1101" : "remote_transmitter/receiver";
#else
  const char *backend = "remote_transmitter/receiver";
#endif
  ESP_LOGCONFIG(TAG,
                "RF Proxy '%s'\n"
                "  Backend: %s\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), backend, YESNO(this->traits_.get_supports_transmitter()),
                YESNO(this->traits_.get_supports_receiver()));

  const auto &traits = this->traits_;
  if (traits.get_frequency_min_hz() > 0) {
    if (traits.get_frequency_min_hz() == traits.get_frequency_max_hz()) {
      ESP_LOGCONFIG(TAG, "  Frequency: %.3f MHz (fixed)", traits.get_frequency_min_hz() / 1e6f);
    } else {
      ESP_LOGCONFIG(TAG, "  Frequency Range: %.3f - %.3f MHz", traits.get_frequency_min_hz() / 1e6f,
                    traits.get_frequency_max_hz() / 1e6f);
    }
  }
}

void RfProxy::control(const radio_frequency::RadioFrequencyCall &call) {
#ifdef USE_CC1101
  // If a CC1101 is the RF front-end:
  // 1. Optionally retune to the requested carrier frequency (skipped if unchanged)
  // 2. Mark TX busy so any sibling RX entity drops ghost events on the shared pin
  // 3. Switch the chip into TX state before handing off to remote_transmitter
  // 4. After transmission completes, return the chip to RX state (idle listening)
  //
  // remote_transmitter::perform() is async — RMT processes the queued symbols in
  // hardware after the call returns.  Calling begin_rx() too early would flip the
  // chip back to RX while the MCU is still pulsing GDO0, corrupting the tail of
  // the packet.  We compute the transmission duration from the timings and use a
  // named scheduler timeout (which auto-cancels if a new TX arrives) to switch
  // back to RX once the in-flight transmission is done.
  if (this->cc1101_ != nullptr) {
    const auto &freq = call.get_frequency();
    if (freq.has_value() && *freq > 0 && *freq != this->last_cc1101_frequency_hz_) {
      ESP_LOGD(TAG, "Retuning CC1101 to %" PRIu32 " Hz", *freq);
      this->cc1101_->set_frequency(static_cast<float>(*freq));
      this->last_cc1101_frequency_hz_ = *freq;
    }
    this->cc1101_->set_tx_busy(true);
    this->cc1101_->begin_tx();
  }
#endif

  uint32_t duration_us = 0;
#ifdef USE_CC1101
  uint32_t *duration_out = this->cc1101_ != nullptr ? &duration_us : nullptr;
#else
  uint32_t *duration_out = nullptr;
#endif
  // RF: no IR carrier modulation
  transmit_raw_timings(this->transmitter_, 0, call, duration_out);

#ifdef USE_CC1101
  if (this->cc1101_ != nullptr) {
    // Round up to ms, plus a small safety margin for RMT processing latency.
    uint32_t duration_ms = (duration_us + 999) / 1000 + 2;
    this->set_timeout("cc1101_rf_tx_done", duration_ms, [this]() {
      this->cc1101_->begin_rx();
      this->cc1101_->set_tx_busy(false);
    });
  }
#endif
}

#endif  // USE_RADIO_FREQUENCY

}  // namespace esphome::ir_rf_proxy
