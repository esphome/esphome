#include "device.h"
#include "decoder.h"
#include "encryption.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome {
namespace bthome {

static const char *TAG = "bthome";

bool DeviceBase::parse_data(MacAddressPtr source_address, const uint8_t *data, size_t data_size) {
  if (this->address_ != source_address) {
    return false;
  }
  BTHomeHeader &header = *(BTHomeHeader *) &data[0];

  const uint8_t *payload;
  size_t payload_size;

#ifdef USE_BTHOME_DECRYPTION
  if (header.encrypted) {
    if (!this->encryption_key.has_value()) {
      ESP_LOGE(TAG, "Encrypted BTHome frame received but no bindkey configured for %s", source_address.c_str());
      return true;
    }

    payload =
        bthome_decrypt(data + 1, data_size - 1, source_address, header, this->encryption_key.value(), payload_size);
    if (payload == nullptr) {
      ESP_LOGVV(TAG, "Failed to decrypt BTHome frame from %s", source_address.c_str());
      return true;
    }
  } else {
    if (this->encryption_key.has_value()) {
      ESP_LOGE(TAG, "Unencrypted BTHome frame received with bindkey configured for %s", source_address.c_str());
      return true;
    }
    payload = data + 1;
    payload_size = data_size - 1;
  }

#else
  if (header.encrypted) {
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

  int index = 0;
  auto handlers = this->get_handlers();
  for (const BTHomeObject &obj : decoder) {
    for (int i = index; i < handlers.size(); i++) {
      if (handlers[i]->process_object(obj)) {
        index = i + 1;
        break;
      }
    }
  }

  return true;
}

}  // namespace bthome
}  // namespace esphome
