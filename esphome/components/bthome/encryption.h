#pragma once

#include "bthome.h"
#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace esphome::bthome {

#ifdef USE_BTHOME_DECRYPTION
bool bthome_decrypt(const uint8_t *ciphertext, size_t ciphertext_size, MacAddressPtr source_address,
                    BTHomeHeader header, const EncryptionKey &key, std::span<uint8_t> plaintext,
                    size_t &plaintext_size);
#endif

}  // namespace esphome::bthome
