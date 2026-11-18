#pragma once

#include <cstdint>

namespace esphome::vnc {

/// DES-ECB encryption of one block in place. The key is in normal DES byte order; parity bits are ignored.
void des_encrypt_block(const uint8_t key[8], uint8_t block[8]);

/// VNC Authentication response: encrypts both 8-byte halves of the challenge in place.
/// key_progmem points to 8 bytes in PROGMEM, already bit-reversed per the VNC convention.
void vnc_auth_response(const uint8_t *key_progmem, uint8_t challenge[16]);

}  // namespace esphome::vnc
