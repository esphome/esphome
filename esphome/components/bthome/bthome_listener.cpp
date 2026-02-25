#include "bthome_listener.h"
#include "bthome_decoder.h"
#include "esphome/core/log.h"

#include <cstring>

#ifdef USE_BTHOME_DECRYPTION
#include "mbedtls/ccm.h"
#endif

namespace esphome {
namespace bthome {

#ifdef USE_BTHOME_DECRYPTION

static constexpr size_t BTHOME_MIC_SIZE = 4;
static constexpr size_t BTHOME_COUNTER_SIZE = 4;
static constexpr size_t BTHOME_NONCE_SIZE = 13;

static uint8_t bthome_decrypted_buf[31];

static bool bthome_decrypt(const uint8_t *data, size_t &data_size, const uint8_t *source_address,
                           const EncryptionKey &key) {
  if (data_size <= 1 + BTHOME_COUNTER_SIZE + BTHOME_MIC_SIZE) {
    ESP_LOGVV(TAG, "Encrypted BTHome payload too short: %zu", data_size);
    return false;
  }

  const size_t ciphertext_size = data_size - 1 - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE;
  if (ciphertext_size > sizeof(bthome_decrypted_buf)) {
    ESP_LOGVV(TAG, "Decrypted BTHome payload too large: %zu", ciphertext_size);
    return false;
  }

  std::array<uint8_t, BTHOME_NONCE_SIZE> nonce{};
  memcpy(nonce.data(), source_address, MAC_ADDRESS_SIZE);
  nonce[6] = 0xD2;
  nonce[7] = 0xFC;
  nonce[8] = data[0];
  memcpy(nonce.data() + 9, &data[data_size - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE], BTHOME_COUNTER_SIZE);

  const uint8_t *ciphertext = data + 1;
  const uint8_t *mic = data + data_size - BTHOME_MIC_SIZE;

  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key.data(), key.size() * 8);
  if (ret) {
    ESP_LOGVV(TAG, "mbedtls_ccm_setkey() failed.");
    mbedtls_ccm_free(&ctx);
    return false;
  }

  ret = mbedtls_ccm_auth_decrypt(&ctx, ciphertext_size, nonce.data(), nonce.size(), nullptr, 0, ciphertext,
                                 bthome_decrypted_buf, mic, BTHOME_MIC_SIZE);
  mbedtls_ccm_free(&ctx);
  if (ret) {
    ESP_LOGVV(TAG, "BTHome decryption failed (ret=%d).", ret);
    return false;
  }

  data_size = ciphertext_size;
  return true;
}

#endif  // USE_BTHOME_DECRYPTION

MacAddress::MacAddress(const uint8_t *addr) { *this = addr; }

MacAddress::MacAddress(uint64_t addr) {
  for (int i = sizeof(this->addr_) - 1; i >= 0; i--) {
    this->addr_[i] = addr & 0xFF;
    addr >>= 8;
  }
}

MacAddress &MacAddress::operator=(const uint8_t *addr) {
  std::memcpy(this->addr_, addr, sizeof(this->addr_));
  return *this;
}

MacAddress::operator const uint8_t *() const { return this->addr_; }

bool MacAddress::operator==(const uint8_t *other) const {
  return std::memcmp(this->addr_, other, sizeof(this->addr_)) == 0;
}
bool MacAddress::operator==(const MacAddress &other) const { return *this == static_cast<const uint8_t *>(other); }
bool MacAddress::operator!=(const uint8_t *other) const { return !(*this == other); }
bool MacAddress::operator!=(const MacAddress &other) const { return !(*this == other); }

bool MacAddressPtr::operator==(const uint8_t *other) const {
  return std::memcmp(this->addr_, other, MAC_ADDRESS_SIZE) == 0;
}
bool MacAddressPtr::operator==(const MacAddressPtr &other) const { return *this == other.addr_; }
bool MacAddressPtr::operator!=(const uint8_t *other) const { return !(*this == other); }
bool MacAddressPtr::operator!=(const MacAddressPtr &other) const { return !(*this == other); }

const char *MacAddressPtr::c_str() const {
  static char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->addr_, buf);
  return buf;
}

const char *MacAddress::c_str() const { return MacAddress(*this).c_str(); }

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

    payload_size = data_size;
    if (!bthome_decrypt(data, payload_size, source_address, this->encryption_key.value())) {
      ESP_LOGVV(TAG, "Failed to decrypt BTHome frame from %s", source_address.c_str());
      return true;
    }
    payload = bthome_decrypted_buf;
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
bool BTHomeSensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(object.as_float());
  return true;
}
}  // namespace bthome
}  // namespace esphome
