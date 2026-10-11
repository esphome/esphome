#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::ota {

// The PSA Crypto API imports an RSA public key as a DER RSAPublicKey
// (RFC 3279 2.3.1), not as raw bignums:
//
//   RSAPublicKey ::= SEQUENCE { modulus INTEGER, publicExponent INTEGER }
//
// The Secure Boot v2 signature block stores the modulus and exponent raw, so
// they are wrapped here. Only RSA-3072 exists in that format, which fixes both
// headers: a 3072-bit modulus always has its top bit set, so its INTEGER is
// always tag + 2-byte length (0x181 = 385) + the sign pad; and the SEQUENCE
// body is always 392..396 bytes, so its header is always tag + 2-byte length.
// Only the exponent varies in width.
constexpr size_t RSA_3072_MODULUS_BYTES = 384;
constexpr uint8_t RSA_DER_MODULUS_PREFIX[] = {0x02, 0x82, 0x01, 0x81, 0x00};
constexpr size_t RSA_DER_MODULUS_LEN = sizeof(RSA_DER_MODULUS_PREFIX) + RSA_3072_MODULUS_BYTES;  // 389
// 4-byte SEQUENCE header + modulus + the widest exponent INTEGER (tag, length,
// sign pad, 4 bytes).
constexpr size_t RSA_DER_PUBKEY_MAX = 4 + RSA_DER_MODULUS_LEN + 7;

/// Wrap a raw RSA-3072 modulus and exponent as a DER RSAPublicKey.
///
/// @param modulus_be   Big-endian modulus, RSA_3072_MODULUS_BYTES long.
/// @param exponent_be  Big-endian exponent, exponent_len bytes, leading zeros allowed.
///                     Rejected if the significant bytes would not fit a short-form length.
/// @return the encoded length, or 0 if the exponent is zero or the buffer is too small.
inline size_t rsa_der_public_key(const uint8_t *modulus_be, const uint8_t *exponent_be, size_t exponent_len,
                                 uint8_t *out, size_t out_len) {
  // A DER INTEGER is signed: drop leading zero bytes, then prepend one back if
  // the value would otherwise read as negative.
  while (exponent_len > 0 && exponent_be[0] == 0x00) {
    exponent_be++;
    exponent_len--;
  }
  if (exponent_len == 0) {
    return 0;  // a zero exponent is not a usable key
  }
  const bool pad = (exponent_be[0] & 0x80) != 0;
  const size_t exponent_content_len = exponent_len + (pad ? 1 : 0);
  if (exponent_content_len > 0x7F) {
    return 0;  // would need a long-form length, which this encoder does not write
  }
  const size_t exponent_der_len = 2 + exponent_content_len;
  const size_t body_len = RSA_DER_MODULUS_LEN + exponent_der_len;
  const size_t total_len = 4 + body_len;
  if (total_len > out_len) {
    return 0;
  }

  size_t i = 0;
  out[i++] = 0x30;  // SEQUENCE
  out[i++] = 0x82;  // 2-byte length follows
  out[i++] = static_cast<uint8_t>(body_len >> 8);
  out[i++] = static_cast<uint8_t>(body_len);
  memcpy(out + i, RSA_DER_MODULUS_PREFIX, sizeof(RSA_DER_MODULUS_PREFIX));
  i += sizeof(RSA_DER_MODULUS_PREFIX);
  memcpy(out + i, modulus_be, RSA_3072_MODULUS_BYTES);
  i += RSA_3072_MODULUS_BYTES;
  out[i++] = 0x02;  // INTEGER
  out[i++] = static_cast<uint8_t>(exponent_content_len);
  if (pad) {
    out[i++] = 0x00;
  }
  memcpy(out + i, exponent_be, exponent_len);
  return total_len;
}

}  // namespace esphome::ota
