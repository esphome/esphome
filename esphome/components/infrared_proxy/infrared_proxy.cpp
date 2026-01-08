#include "infrared_proxy.h"
#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::infrared_proxy {

static const char *const TAG = "infrared_proxy";

void InfraredProxyComponent::setup() {
#ifdef USE_API
  // Register with API server
  if (api::global_api_server != nullptr) {
    api::global_api_server->register_infrared_proxy(this);
  }
#endif
}

void InfraredProxyComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Infrared Proxy:\n"
                "  Has Transmitter: %s\n"
                "  Has Receiver: %s",
                YESNO(this->has_transmitter()), YESNO(this->has_receiver()));
  if (this->is_rf()) {
    ESP_LOGCONFIG(TAG, "  Hardware Type: RF (%.3f MHz)", this->frequency_ / 1e3f);
  } else {
    ESP_LOGCONFIG(TAG, "  Hardware Type: Infrared");
  }
}

#ifdef USE_API
uint32_t InfraredProxyComponent::get_capability_flags() const {
  uint32_t flags = 0;

  // Add transmit/receive capability based on configuration
  if (this->has_transmitter())
    flags |= InfraredProxyCapability::CAPABILITY_TRANSMITTER;
  if (this->has_receiver())
    flags |= InfraredProxyCapability::CAPABILITY_RECEIVER;

  return flags;
}

void InfraredProxyComponent::transmit_raw_timings(const api::InfraredProxyTransmitRawTimingsRequest &msg) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (msg.timings.empty()) {
    ESP_LOGE(TAG, "Raw timings array is empty");
    return;
  }

  ESP_LOGD(TAG, "Transmitting raw timings: key=%u, carrier=%u Hz, timing_count=%u, repeat_count=%u", msg.key,
           msg.carrier_frequency, msg.timings.size(), msg.repeat_count);

  // Create transmit data object
  auto call = this->transmitter_->transmit();
  auto *transmit_data = call.get_data();

  // Set carrier frequency (0 = no carrier for RF applications)
  transmit_data->set_carrier_frequency(msg.carrier_frequency);

  // Move raw timings to avoid copying - the remote_base protocol expects alternating mark/space
  // Positive values = mark (LED on), negative values = space (LED off)
  // FixedVector conversion operator creates a temporary std::vector which is automatically moved
  transmit_data->set_data(static_cast<std::vector<int32_t>>(msg.timings));

  // Set repeat count (default to 1 if not specified or 0)
  if (msg.repeat_count > 0) {
    call.set_send_times(msg.repeat_count);
  }

  // Transmit the data
  call.perform();
}

bool InfraredProxyComponent::on_receive(remote_base::RemoteReceiveData data) {
  if (this->receiver_ == nullptr) {
    return false;  // Not interested in receive data if no receiver configured
  }

  // Get the raw timings
  const auto &raw_data = data.get_raw_data();

  ESP_LOGD(TAG, "Measured %u timings", raw_data.size());

  // Send the raw timings to the API
  api::global_api_server->send_infrared_proxy_receive_event(this->get_object_id_hash(), raw_data);

  return false;  // Return false to allow other listeners to process the data
}
#endif  // USE_API

}  // namespace esphome::infrared_proxy
