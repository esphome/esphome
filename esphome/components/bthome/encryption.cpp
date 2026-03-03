#include "encryption.h"

#if defined(USE_BTHOME_DECRYPTION) || defined(USE_BTHOME_ENCRYPTION)

#include "esphome/core/log.h"
#include "mbedtls/ccm.h"

namespace esphome {
namespace bthome {

static const char *TAG = "bthome";

static constexpr size_t BTHOME_MIC_SIZE = 4;
static constexpr size_t BTHOME_COUNTER_SIZE = 4;
static constexpr size_t BLE_FLAGS_SIZE = 3;
static constexpr size_t BLE_ADV_HEADER_SIZE = 4;
static constexpr uint16_t BTHOME_UUID16 = 0xFCD2;
static constexpr size_t BLE_ADV_MAX_SIZE = 31;
static constexpr size_t BTHOME_MAX_ENCRYPTED_PAYLOAD = BLE_ADV_MAX_SIZE - BLE_FLAGS_SIZE - BLE_ADV_HEADER_SIZE -
                                                       sizeof(esphome::bthome::BTHomeHeader) - BTHOME_COUNTER_SIZE -
                                                       BTHOME_MIC_SIZE;

static uint8_t bthome_encryption_buf[BTHOME_MAX_ENCRYPTED_PAYLOAD + BTHOME_COUNTER_SIZE + BTHOME_MIC_SIZE];

struct __attribute__((packed)) BTHomeNonce {
  MacAddress mac_address;           // 6 bytes
  uint16_t uuid16 = BTHOME_UUID16;  // 2 bytes: BTHome UUID (0xFCD2 in LE)
  BTHomeHeader header;              // 1 byte: sw_version from BTHome header
  uint32_t counter;                 // 4 bytes: packet counter
};
static_assert(sizeof(BTHomeNonce) == 13, "BTHomeNonce must be exactly 13 bytes");

const uint8_t *bthome_encrypt(const uint8_t *plaintext, size_t plaintext_size, MacAddressPtr source_address,
                              uint32_t counter, BTHomeHeader header, const EncryptionKey &key, size_t &encrypted_size) {
  encrypted_size = plaintext_size + BTHOME_COUNTER_SIZE + BTHOME_MIC_SIZE;
  if (encrypted_size > sizeof(bthome_encryption_buf)) {
    ESP_LOGVV(TAG, "Encrypted BTHome plaintext too large: %zu", encrypted_size);
    return nullptr;
  }

  // Build nonce from components
  BTHomeNonce nonce{.mac_address = source_address, .header = header, .counter = counter};

  // Encrypt using AES-CCM
  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key.data(), key.size() * 8);
  if (ret) {
    ESP_LOGVV(TAG, "mbedtls_ccm_setkey() failed.");
    mbedtls_ccm_free(&ctx);
    return nullptr;
  }

  uint8_t *ciphertext = bthome_encryption_buf;
  uint8_t *counter_ptr = ciphertext + plaintext_size;
  uint8_t *mic = counter_ptr + BTHOME_COUNTER_SIZE;

  // Copy counter
  *(uint32_t *) counter_ptr = counter;

  ret = mbedtls_ccm_encrypt_and_tag(&ctx, plaintext_size, (const uint8_t *) &nonce, sizeof(nonce), nullptr, 0,
                                    plaintext, ciphertext, mic, BTHOME_MIC_SIZE);
  mbedtls_ccm_free(&ctx);
  if (ret) {
    ESP_LOGVV(TAG, "BTHome encryption failed (ret=%d).", ret);
    return nullptr;
  }

  return bthome_encryption_buf;
}

const uint8_t *bthome_decrypt(const uint8_t *ciphertext, size_t ciphertext_size, MacAddressPtr source_address,
                              BTHomeHeader header, const EncryptionKey &key, size_t &plaintext_size) {
  if (ciphertext_size <= BTHOME_COUNTER_SIZE + BTHOME_MIC_SIZE) {
    ESP_LOGVV(TAG, "Encrypted BTHome payload too short: %zu", ciphertext_size);
    return nullptr;
  }

  plaintext_size = ciphertext_size - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE;
  if (plaintext_size > sizeof(bthome_encryption_buf)) {
    ESP_LOGVV(TAG, "Decrypted BTHome payload too large: %zu", plaintext_size);
    return nullptr;
  }

  // Extract counter from the ciphertext (stored before the MIC)
  uint32_t counter = *(uint32_t *) &ciphertext[ciphertext_size - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE];

  // Build nonce from components
  BTHomeNonce nonce{.mac_address = source_address, .header = header, .counter = counter};

  const uint8_t *mic = ciphertext + ciphertext_size - BTHOME_MIC_SIZE;

  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key.data(), key.size() * 8);
  if (ret) {
    ESP_LOGVV(TAG, "mbedtls_ccm_setkey() failed.");
    mbedtls_ccm_free(&ctx);
    return nullptr;
  }

  ret = mbedtls_ccm_auth_decrypt(&ctx, plaintext_size, (const uint8_t *) &nonce, sizeof(nonce), nullptr, 0, ciphertext,
                                 bthome_encryption_buf, mic, BTHOME_MIC_SIZE);
  mbedtls_ccm_free(&ctx);
  if (ret) {
    ESP_LOGVV(TAG, "BTHome decryption failed (ret=%d).", ret);
    return nullptr;
  }

  return bthome_encryption_buf;
}

}  // namespace bthome
}  // namespace esphome

#endif  // USE_BTHOME_DECRYPTION || USE_BTHOME_ENCRYPTION
