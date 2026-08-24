#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/components/ota/ota_rsa_der.h"

namespace esphome::ota::testing {

namespace {

// A modulus with the top bit set, as every real 3072-bit modulus has.
std::array<uint8_t, RSA_3072_MODULUS_BYTES> make_modulus(uint8_t first = 0xC5) {
  std::array<uint8_t, RSA_3072_MODULUS_BYTES> modulus{};
  modulus.fill(0xAB);
  modulus[0] = first;
  modulus[RSA_3072_MODULUS_BYTES - 1] = 0x01;  // odd, like a real modulus
  return modulus;
}

}  // namespace

// e = 65537, the exponent espsecure uses.
TEST(RsaDerPublicKey, StandardExponent) {
  const auto modulus = make_modulus();
  const uint8_t exponent[4] = {0x00, 0x01, 0x00, 0x01};
  uint8_t der[RSA_DER_PUBKEY_MAX];
  const size_t len = rsa_der_public_key(modulus.data(), exponent, sizeof(exponent), der, sizeof(der));

  // 4 (SEQUENCE header) + 389 (modulus) + 5 (exponent) = 398
  ASSERT_EQ(len, 398u);
  // SEQUENCE, 2-byte length of the 394-byte body
  EXPECT_EQ(der[0], 0x30);
  EXPECT_EQ(der[1], 0x82);
  EXPECT_EQ((der[2] << 8) | der[3], 394);
  // INTEGER, 2-byte length 385, sign pad, then the modulus
  EXPECT_EQ(der[4], 0x02);
  EXPECT_EQ(der[5], 0x82);
  EXPECT_EQ((der[6] << 8) | der[7], 385);
  EXPECT_EQ(der[8], 0x00);
  EXPECT_EQ(0, memcmp(der + 9, modulus.data(), modulus.size()));
  // INTEGER, 3 bytes, leading zero of the input dropped
  const size_t exp_at = 9 + RSA_3072_MODULUS_BYTES;
  EXPECT_EQ(der[exp_at], 0x02);
  EXPECT_EQ(der[exp_at + 1], 0x03);
  EXPECT_EQ(der[exp_at + 2], 0x01);
  EXPECT_EQ(der[exp_at + 3], 0x00);
  EXPECT_EQ(der[exp_at + 4], 0x01);
}

// An exponent whose top bit is set needs a 0x00 sign pad, widening the body.
TEST(RsaDerPublicKey, ExponentNeedingSignPad) {
  const auto modulus = make_modulus();
  const uint8_t exponent[4] = {0x00, 0x00, 0x00, 0x81};
  uint8_t der[RSA_DER_PUBKEY_MAX];
  const size_t len = rsa_der_public_key(modulus.data(), exponent, sizeof(exponent), der, sizeof(der));

  ASSERT_EQ(len, 397u);  // 4 + 389 + 4
  const size_t exp_at = 9 + RSA_3072_MODULUS_BYTES;
  EXPECT_EQ(der[exp_at], 0x02);
  EXPECT_EQ(der[exp_at + 1], 0x02);  // pad + one value byte
  EXPECT_EQ(der[exp_at + 2], 0x00);
  EXPECT_EQ(der[exp_at + 3], 0x81);
}

// The widest exponent still fits the documented buffer size.
TEST(RsaDerPublicKey, WidestExponentFitsBuffer) {
  const auto modulus = make_modulus();
  const uint8_t exponent[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t der[RSA_DER_PUBKEY_MAX];
  const size_t len = rsa_der_public_key(modulus.data(), exponent, sizeof(exponent), der, sizeof(der));

  ASSERT_EQ(len, RSA_DER_PUBKEY_MAX);  // 4 + 389 + 7
  EXPECT_LE(len, sizeof(der));
}

TEST(RsaDerPublicKey, ZeroExponentRejected) {
  const auto modulus = make_modulus();
  const uint8_t exponent[4] = {0x00, 0x00, 0x00, 0x00};
  uint8_t der[RSA_DER_PUBKEY_MAX];
  EXPECT_EQ(rsa_der_public_key(modulus.data(), exponent, sizeof(exponent), der, sizeof(der)), 0u);
}

// A buffer that cannot hold the result must be refused, not overrun. Sized
// against a heap vector so ASAN catches a write past the end.
TEST(RsaDerPublicKey, ShortBufferRejected) {
  const auto modulus = make_modulus();
  const uint8_t exponent[4] = {0x00, 0x01, 0x00, 0x01};
  for (size_t out_len : {size_t(0), size_t(1), size_t(4), size_t(100), size_t(397)}) {
    std::vector<uint8_t> der(out_len);
    EXPECT_EQ(rsa_der_public_key(modulus.data(), exponent, sizeof(exponent), der.data(), out_len), 0u)
        << "out_len=" << out_len;
  }
}

}  // namespace esphome::ota::testing
