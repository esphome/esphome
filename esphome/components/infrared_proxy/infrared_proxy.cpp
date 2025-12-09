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
}

#ifdef USE_API

uint32_t InfraredProxyComponent::get_capability_flags() const {
  uint32_t flags = InfraredProxyCapability::CAPABILITY_RADIO_FREQ;  // Always included

  if (this->has_transmitter())
    flags |= InfraredProxyCapability::CAPABILITY_TRANSMITTER;
  if (this->has_receiver())
    flags |= InfraredProxyCapability::CAPABILITY_RECEIVER;

  return flags;
}

void InfraredProxyComponent::transmit(const api::InfraredProxyTransmitRequest &msg) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "Cannot transmit: no transmitter configured");
    return;
  }

  // Convert protobuf data pointer to vector
  std::vector<uint8_t> data_vec(msg.data, msg.data + msg.data_len);

  ESP_LOGD(TAG, "Transmitting IR data: key=%u, frequency=%u, bits=%u, data_size=%u", msg.key, msg.timing.frequency,
           msg.timing.length_in_bits, data_vec.size());

  // Create transmit data object
  auto call = this->transmitter_->transmit();
  auto *transmit_data = call.get_data();

  // Set carrier frequency (0 = no carrier for RF applications)
  transmit_data->set_carrier_frequency(msg.timing.frequency);

  // Encode the data into raw timings
  this->encode_data_(msg.timing, data_vec, transmit_data);

  // Transmit the encoded data
  call.perform();
}

void InfraredProxyComponent::encode_data_(const api::InfraredProxyTimingParams &timing,
                                          const std::vector<uint8_t> &data,
                                          remote_base::RemoteTransmitData *transmit_data) {
  // Send header if specified
  if (timing.header_high_us > 0 || timing.header_low_us > 0) {
    transmit_data->item(timing.header_high_us, timing.header_low_us);
  }

  // Calculate total number of bits to send
  uint32_t total_bits = timing.length_in_bits > 0 ? timing.length_in_bits : (data.size() * 8);

  // Encode data bits
  for (uint32_t bit_index = 0; bit_index < total_bits; bit_index++) {
    // Calculate byte and bit position
    uint32_t byte_index;
    uint32_t bit_position;

    if (timing.msb_first) {
      // MSB first: bit 0 is the MSB of byte 0
      byte_index = bit_index / 8;
      bit_position = 7 - (bit_index % 8);
    } else {
      // LSB first: bit 0 is the LSB of byte 0
      byte_index = bit_index / 8;
      bit_position = bit_index % 8;
    }

    // Check if we have enough data
    if (byte_index >= data.size()) {
      ESP_LOGW(TAG, "Data underrun: need %u bits but only have %u bytes", total_bits, data.size());
      break;
    }

    // Extract bit value
    bool bit_value = (data[byte_index] >> bit_position) & 0x01;

    // Send appropriate timing based on bit value
    if (bit_value) {
      transmit_data->item(timing.one_high_us, timing.one_low_us);
    } else {
      transmit_data->item(timing.zero_high_us, timing.zero_low_us);
    }
  }

  // Send footer if specified
  if (timing.footer_high_us > 0 || timing.footer_low_us > 0) {
    transmit_data->item(timing.footer_high_us, timing.footer_low_us);
  }

  // Handle repeat transmissions
  if (timing.repeat_count > 1) {
    for (uint32_t repeat = 1; repeat < timing.repeat_count; repeat++) {
      // Add minimum idle time between repeats
      if (timing.minimum_idle_time_us > 0) {
        transmit_data->space(timing.minimum_idle_time_us);
      }

      // Send repeat header if specified
      if (timing.repeat_high_us > 0 || timing.repeat_low_us > 0) {
        transmit_data->item(timing.repeat_high_us, timing.repeat_low_us);
      } else if (timing.header_high_us > 0 || timing.header_low_us > 0) {
        // If no repeat header specified, use the normal header
        transmit_data->item(timing.header_high_us, timing.header_low_us);
      }

      // Re-encode data bits for repeat
      for (uint32_t bit_index = 0; bit_index < total_bits; bit_index++) {
        uint32_t byte_index;
        uint32_t bit_position;

        if (timing.msb_first) {
          byte_index = bit_index / 8;
          bit_position = 7 - (bit_index % 8);
        } else {
          byte_index = bit_index / 8;
          bit_position = bit_index % 8;
        }

        if (byte_index >= data.size())
          break;

        bool bit_value = (data[byte_index] >> bit_position) & 0x01;

        if (bit_value) {
          transmit_data->item(timing.one_high_us, timing.one_low_us);
        } else {
          transmit_data->item(timing.zero_high_us, timing.zero_low_us);
        }
      }

      // Send footer for repeat
      if (timing.footer_high_us > 0 || timing.footer_low_us > 0) {
        transmit_data->item(timing.footer_high_us, timing.footer_low_us);
      }
    }
  }
}

bool InfraredProxyComponent::on_receive(remote_base::RemoteReceiveData data) {
  if (this->receiver_ == nullptr) {
    return false;  // Not interested in receive data if no receiver configured
  }

  // Get the raw timings
  const auto &raw_data = data.get_raw_data();

  ESP_LOGD(TAG, "Received %u timings", raw_data.size());

  // Send the raw timings to the API
  api::global_api_server->send_infrared_proxy_receive_event(this->get_object_id_hash(), raw_data);

  return false;  // Return false to allow other listeners to process the data
}

#endif  // USE_API

}  // namespace esphome::infrared_proxy
