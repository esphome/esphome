#include "transmitter_radio_frequency.h"

#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::remote_transmitter {

#ifdef USE_RADIO_FREQUENCY
static const char *const TAG = "remote_transmitter.radio_frequency";

void TransmitterRadioFrequency::setup() { this->traits_.set_supports_transmitter(true); }

void TransmitterRadioFrequency::control(const radio_frequency::RadioFrequencyCall &call) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGE(TAG, "No transmitter configured!");
    return;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGW(TAG, "RadioFrequencyCall has no raw timings to transmit.");
    return;
  }

  auto tx_call = this->transmitter_->transmit();
  auto *tx_data = tx_call.get_data();

  if (call.is_packed()) {
    tx_data->set_data_from_packed_sint32(call.get_packed_data(), call.get_packed_length(), call.get_packed_count());
  } else if (call.is_base64url()) {
    if (!tx_data->set_data_from_base64url(call.get_base64url_data())) {
      ESP_LOGE(TAG, "Failed to decode base64url data");
      return;
    }
    constexpr int32_t max_timing_us = 500000;
    for (int32_t timing : tx_data->get_data()) {
      int32_t abs_timing = timing < 0 ? -timing : timing;
      if (abs_timing > max_timing_us) {
        ESP_LOGE(TAG, "Invalid timing value: %" PRId32 " µs (max %" PRId32 ")", timing, max_timing_us);
        return;
      }
    }
    ESP_LOGD(TAG, "Transmitting base64url raw timings: count=%zu, repeat=%" PRIu32, tx_data->get_data().size(),
             call.get_repeat_count());
  } else {
    tx_data->set_data(call.get_raw_timings());
  }

  if (call.get_frequency().has_value()) {
    tx_data->set_carrier_frequency(*call.get_frequency());
  }

  if (call.get_repeat_count() > 0) {
    tx_call.set_send_times(call.get_repeat_count());
  }
  tx_call.perform();
}
#endif  // USE_RADIO_FREQUENCY

}  // namespace esphome::remote_transmitter
