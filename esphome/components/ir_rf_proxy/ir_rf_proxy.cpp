#include "ir_rf_proxy.h"
#include "esphome/core/log.h"

namespace esphome::ir_rf_proxy {

static const char *const TAG = "ir_rf_proxy";

void IrRfProxy::setup() {
  // Set up traits based on configuration
  this->traits_.set_supports_transmitter(this->has_transmitter());
  this->traits_.set_supports_receiver(this->has_receiver());

  // Call base class setup (handles API registration via ComponentIterator)
  infrared::Infrared::setup();
}

void IrRfProxy::dump_config() {
  ESP_LOGCONFIG(TAG, "IR/RF Proxy (remote_base platform):");
  ESP_LOGCONFIG(TAG, "  Has Transmitter: %s", YESNO(this->has_transmitter()));
  ESP_LOGCONFIG(TAG, "  Has Receiver: %s", YESNO(this->has_receiver()));

  if (this->is_rf()) {
    ESP_LOGCONFIG(TAG, "  Hardware Type: RF (%.3f MHz)", this->frequency_khz_ / 1e3f);
  } else {
    ESP_LOGCONFIG(TAG, "  Hardware Type: Infrared");
  }
}

void IrRfProxy::set_remote_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
  // Cast to base type for infrared::Infrared
  this->set_transmitter(static_cast<remote_base::RemoteTransmitterBase *>(transmitter));
}

void IrRfProxy::set_remote_receiver(remote_receiver::RemoteReceiverComponent *receiver) {
  // Cast to base type for infrared::Infrared
  this->set_receiver(static_cast<remote_base::RemoteReceiverBase *>(receiver));
}

void IrRfProxy::control(const infrared::InfraredCall &call) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGE(TAG, "No raw timings provided");
    return;
  }

  // Cast back to remote_transmitter for the call
  auto *tx = static_cast<remote_transmitter::RemoteTransmitterComponent *>(this->transmitter_);
  auto tx_call = tx->transmit();
  auto *transmit_data = tx_call.get_data();

  // Set carrier frequency if provided (0 = no carrier for RF)
  if (call.get_carrier_frequency().has_value()) {
    transmit_data->set_carrier_frequency(call.get_carrier_frequency().value());
  }

  // Handle raw timings - check if using packed format (from API) or vector format (from lambdas)
  if (call.is_packed()) {
    // Zero-copy: decode directly from protobuf packed buffer
    transmit_data->set_data_from_packed_sint32(call.get_packed_data(), call.get_packed_length(),
                                               call.get_packed_count());
    ESP_LOGD(TAG, "Transmitting packed raw timings: count=%u, repeat=%u", call.get_packed_count(),
             call.get_repeat_count());
  } else {
    // Copy from vector (used by lambdas/automations)
    transmit_data->set_data(call.get_raw_timings());
    ESP_LOGD(TAG, "Transmitting raw timings: count=%u, repeat=%u", call.get_raw_timings().size(),
             call.get_repeat_count());
  }

  // Set repeat count
  if (call.get_repeat_count() > 1) {
    tx_call.set_send_times(call.get_repeat_count());
  }

  // Perform the transmission
  tx_call.perform();
}

}  // namespace esphome::ir_rf_proxy
