#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include "esphome/core/defines.h"

#ifdef USE_BTHOME_DECRYPTION
#include "bthome_device.h"
#include "bthome_mac.h"

namespace esphome {
namespace bthome {

// Encrypt BTHome payload data
// Returns pointer to encrypted packet on success, nullptr on failure
// encrypted_size is set to the total size (ciphertext + counter + mic), header is NOT included
const uint8_t *bthome_encrypt(const uint8_t *plaintext, size_t plaintext_size, MacAddressPtr source_address,
                              uint32_t counter, BTHomeHeader header, const EncryptionKey &key, size_t &ciphertext_size);

// Decrypt BTHome encrypted advertisement data
// Returns pointer to decrypted payload on success, nullptr on failure
// plaintext_size is set to the size of the decrypted payload
const uint8_t *bthome_decrypt(const uint8_t *ciphertext, size_t ciphertext_size, MacAddressPtr source_address,
                              BTHomeHeader header, const EncryptionKey &key, size_t &plaintext_size);

}  // namespace bthome
}  // namespace esphome

#endif  // USE_BTHOME_DECRYPTION
