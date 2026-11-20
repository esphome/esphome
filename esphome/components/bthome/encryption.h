#pragma once

#include "bthome.h"
#include "esphome/core/defines.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace esphome::bthome {

#ifdef USE_BTHOME_DECRYPTION
using EncryptionKey = std::array<uint8_t, 16>;

static constexpr size_t BTHOME_MIC_SIZE = 4;
static constexpr size_t BTHOME_COUNTER_SIZE = 4;
// Maximum plaintext in a legacy 31-byte advertisement with flags, service header, counter, and MIC.
static constexpr size_t BTHOME_MAX_ENCRYPTED_PAYLOAD = 15;

bool bthome_decrypt(const uint8_t *ciphertext, size_t ciphertext_size, const uint8_t *source_address,
                    BTHomeHeader header, const EncryptionKey &key, std::span<uint8_t> plaintext,
                    size_t &plaintext_size);
#endif

}  // namespace esphome::bthome
