#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::ble_device_base {

// Self-contained AES-128-CCM authenticated decryption (RFC 3610).
//
// Encrypted BLE advertisements (BTHome, several Xiaomi/ATC variants) use
// AES-128-CCM. The platform crypto that provides it is inconsistent across BLE
// targets: ESP-IDF exposes PSA/mbedtls, but a LibreTiny SDK may keep its mbedtls
// internal (e.g. the beken-72xx SDK ships mbedtls with CCM enabled but does not
// put it on the application include path), so a sensor cannot rely on
// <mbedtls/ccm.h> being available. This software implementation makes
// encrypted-advertisement decryption work on every BLE platform without a
// per-chip crypto dependency. Decryption volume is tiny (one short block per
// matching advertisement), so software AES is not a meaningful cost.
//
// Verifies the CCM authentication tag and, on success, writes `ct_len` decrypted
// bytes to `plaintext` and returns true. Returns false when authentication fails
// (the caller must then discard `plaintext`). The CCM parameters follow the
// caller (BTHome: 13-byte nonce, 4-byte tag, no associated data); `aad` may be
// null when `aad_len` is 0.
/// AES-128 single-block encrypt (the same software cipher CCM uses). Used by
/// ESPBTDevice::resolve_irk() for the Bluetooth "ah" RPA hash, so IRK matching
/// works identically on every platform with no chip crypto dependency.
void aes128_encrypt_block(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

bool aes_ccm_auth_decrypt(const uint8_t key[16], const uint8_t *nonce, size_t nonce_len, const uint8_t *aad,
                          size_t aad_len, const uint8_t *ciphertext, size_t ct_len, uint8_t *plaintext,
                          const uint8_t *tag, size_t tag_len);

}  // namespace esphome::ble_device_base
