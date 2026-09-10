#include "remote_device.h"
#include "decoder.h"
#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/core/log.h"

#include <array>
#include <cstring>

namespace esphome::bthome::client {

static const char *const TAG = "bthome";

bool RemoteDeviceBase::parse_data(MacAddressPtr source_address, const uint8_t *data, size_t data_size) {
  if (this->address_ != source_address) {
    return false;
  }
  if (data_size == 0) {
    ESP_LOGVV(TAG, "BTHome data is empty");
    return true;
  }

  const BTHomeHeader header{data[0]};

  const uint8_t *payload;
  size_t payload_size;

#ifdef USE_BTHOME_DECRYPTION
  std::array<uint8_t, BTHOME_MAX_ENCRYPTED_PAYLOAD> decrypted_payload{};
  if (header.encrypted()) {
    if (!this->encryption_key_.has_value()) {
      ESP_LOGE(TAG, "Encrypted BTHome frame received but no bindkey configured for %s", source_address.c_str());
      return true;
    }

    if (data_size <= 1 + BTHOME_COUNTER_SIZE + BTHOME_MIC_SIZE) {
      ESP_LOGVV(TAG, "Encrypted BTHome payload too short: %zu", data_size - 1);
      return true;
    }

    payload_size = data_size - 1 - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE;
    if (payload_size > decrypted_payload.size()) {
      ESP_LOGVV(TAG, "Decrypted BTHome payload too large: %zu", payload_size);
      return true;
    }

    std::array<uint8_t, 13> nonce{};
    std::memcpy(nonce.data(), static_cast<const uint8_t *>(source_address), MAC_ADDRESS_SIZE);
    nonce[6] = BTHOME_SVC_UUID_LOW;
    nonce[7] = BTHOME_SVC_UUID_HIGH;
    nonce[8] = header.data;
    const uint8_t *counter = data + 1 + payload_size;
    std::memcpy(nonce.data() + 9, counter, BTHOME_COUNTER_SIZE);
    const uint8_t *mic = counter + BTHOME_COUNTER_SIZE;

    if (!ble_device_base::aes_ccm_auth_decrypt(this->encryption_key_->data(), nonce.data(), nonce.size(), nullptr, 0,
                                               data + 1, payload_size, decrypted_payload.data(), mic,
                                               BTHOME_MIC_SIZE)) {
      ESP_LOGVV(TAG, "Failed to decrypt BTHome frame from %s", source_address.c_str());
      return true;
    }
    payload = decrypted_payload.data();
  } else {
    if (this->encryption_key_.has_value()) {
      ESP_LOGE(TAG, "Unencrypted BTHome frame received with bindkey configured for %s", source_address.c_str());
      return true;
    }
    payload = data + 1;
    payload_size = data_size - 1;
  }

#else
  if (header.encrypted()) {
    ESP_LOGE(TAG, "Encrypted BTHome frame received but no bindkey configured for %s", source_address.c_str());
    return true;
  }
  payload = data + 1;
  payload_size = data_size - 1;
#endif

  BTHomePayloadDecoder decoder(payload, payload_size);

  // Ignore repeated packets using the optional packet ID field (object type 0x00).
  // Since objects are in ascending order, PACKET_ID always appears first if present.
  auto it = decoder.begin();
  if (it != decoder.end() && (*it).type == BTHomeObjectType::PACKET_ID) {
    uint8_t packet_id = (*it).data[0];
    if (this->last_packet_id_.has_value() && this->last_packet_id_.value() == packet_id) {
      ESP_LOGVV(TAG, "Duplicate packet ID %u from %s, ignoring", packet_id, source_address.c_str());
      return true;
    }
    this->last_packet_id_ = packet_id;
  }

  size_t index = 0;
  auto handlers = this->get_handlers();
  for (const BTHomeObject &obj : decoder) {
    for (size_t i = index; i < handlers.size(); i++) {
      if (handlers[i]->process_object(obj)) {
        index = i + 1;
        break;
      }
    }
  }

  return true;
}

}  // namespace esphome::bthome::client
