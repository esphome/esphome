#include "ir_rf_proxy.h"

#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::ir_rf_proxy {

static const char *const TAG = "ir_rf_proxy";

// ========== Shared transmit helper ==========
// Static template: all instantiations occur in this translation unit.

template<typename CallT>
static bool transmit_raw_timings(remote_base::RemoteTransmitterBase *transmitter, uint32_t carrier_frequency,
                                 const CallT &call, uint32_t &inflight_seq) {
  if (transmitter == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return false;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGE(TAG, "No raw timings provided");
    return false;
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
      return false;
    }
    constexpr int32_t max_timing_us = 500000;
    for (int32_t timing : transmit_data->get_data()) {
      int32_t abs_timing = timing < 0 ? -timing : timing;
      if (abs_timing > max_timing_us) {
        ESP_LOGE(TAG, "Invalid timing value: %" PRId32 " µs (max %" PRId32 ")", timing, max_timing_us);
        return false;
      }
    }
    ESP_LOGD(TAG, "Transmitting base64url raw timings: count=%zu, repeat=%" PRIu32, transmit_data->get_data().size(),
             call.get_repeat_count());
  } else {
    transmit_data->set_data(call.get_raw_timings());
    ESP_LOGD(TAG, "Transmitting raw timings: count=%zu, repeat=%" PRIu32, call.get_raw_timings().size(),
             call.get_repeat_count());
  }

  if (call.get_repeat_count() > 0) {
    transmit_call.set_send_times(call.get_repeat_count());
  }

  inflight_seq = transmit_call.get_seq();
  transmit_call.perform();
  return true;
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

bool IrRfProxy::control(const infrared::InfraredCall &call) {
  uint32_t carrier = call.get_carrier_frequency().value_or(0);
  return transmit_raw_timings(this->transmitter_, carrier, call, this->inflight_seq_);
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
#if defined(USE_API) && defined(USE_RADIO_FREQUENCY)
  if (this->transmitter_ != nullptr) {
    // only frames this entity submitted; YAML automations share the transmitter
    this->transmitter_->set_on_complete_callback([this](uint32_t seq) {
      if (seq == this->inflight_seq_)
        this->notify_transmit_complete_();
    });
  }
#endif
}

void RfProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RF Proxy '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->traits_.get_supports_transmitter()),
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

bool RfProxy::control(const radio_frequency::RadioFrequencyCall &call) {
  // RF: no IR carrier modulation.  Any RF front-end coordination (state turnaround, retuning)
  // happens via the radio_frequency entity's on_control trigger and remote_transmitter's
  // on_transmit/on_complete triggers — wired up in user YAML.
  return transmit_raw_timings(this->transmitter_, 0, call, this->inflight_seq_);
}

#endif  // USE_RADIO_FREQUENCY

}  // namespace esphome::ir_rf_proxy
