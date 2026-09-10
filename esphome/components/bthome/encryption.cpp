#include "encryption.h"

#ifdef USE_BTHOME_DECRYPTION

#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/core/log.h"

#include <array>
#include <cstring>

namespace esphome::bthome {

static const char *const TAG = "bthome";

bool bthome_decrypt(const uint8_t *ciphertext, size_t ciphertext_size, MacAddressPtr source_address,
                    BTHomeHeader header, const EncryptionKey &key, std::span<uint8_t> plaintext,
                    size_t &plaintext_size) {
  if (ciphertext_size <= BTHOME_COUNTER_SIZE + BTHOME_MIC_SIZE) {
    ESP_LOGVV(TAG, "Encrypted BTHome payload too short: %zu", ciphertext_size);
    return false;
  }

  plaintext_size = ciphertext_size - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE;
  if (plaintext_size > plaintext.size()) {
    ESP_LOGVV(TAG, "Decrypted BTHome payload too large: %zu", plaintext_size);
    return false;
  }

  std::array<uint8_t, 13> nonce{};
  std::memcpy(nonce.data(), static_cast<const uint8_t *>(source_address), MAC_ADDRESS_SIZE);
  nonce[6] = BTHOME_SVC_UUID_LOW;
  nonce[7] = BTHOME_SVC_UUID_HIGH;
  nonce[8] = header.data;
  const uint8_t *counter = ciphertext + plaintext_size;
  std::memcpy(nonce.data() + 9, counter, BTHOME_COUNTER_SIZE);
  const uint8_t *mic = counter + BTHOME_COUNTER_SIZE;

  return ble_device_base::aes_ccm_auth_decrypt(key.data(), nonce.data(), nonce.size(), nullptr, 0, ciphertext,
                                               plaintext_size, plaintext.data(), mic, BTHOME_MIC_SIZE);
}

}  // namespace esphome::bthome

#endif  // USE_BTHOME_DECRYPTION
