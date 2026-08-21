#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/ble_device_base/ble_aes_ccm.h"

namespace esphome::ble_device_base::testing {

// Reference vector generated with Python `cryptography` AESCCM(tag_length=4),
// using the same AES-128-CCM parameters BTHome advertisements use: a 16-byte
// key, a 13-byte nonce, a 4-byte authentication tag and no associated data.
namespace {
const uint8_t KEY[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
const uint8_t NONCE[13] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c};
const uint8_t CIPHERTEXT[7] = {0x68, 0xb4, 0xf6, 0xc5, 0x2b, 0xf8, 0xaf};
const uint8_t TAG[4] = {0x48, 0x4d, 0xaa, 0x56};
const uint8_t PLAINTEXT[7] = {0x02, 0x01, 0x64, 0x03, 0x10, 0x8a, 0x01};
}  // namespace

// Xiaomi's parameters differ from BTHome's: a 12-byte nonce and a 1-byte AAD
// (0x11). Both the AAD block and the l = 3 length encoding are only reachable
// through this shape, so they need their own vector. Generated the same way.
namespace {
const uint8_t NONCE_XIAOMI[12] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b};
const uint8_t AAD_XIAOMI[1] = {0x11};
const uint8_t CIPHERTEXT_XIAOMI[5] = {0xc3, 0x7e, 0x0a, 0x1d, 0x23};
const uint8_t TAG_XIAOMI[4] = {0x98, 0x79, 0x87, 0xc6};
const uint8_t PLAINTEXT_XIAOMI[5] = {0x04, 0x10, 0x02, 0xd4, 0x00};
}  // namespace

TEST(BleAesCcm, DecryptsXiaomiShapedVector) {
  uint8_t out[sizeof(PLAINTEXT_XIAOMI)] = {};
  EXPECT_TRUE(aes_ccm_auth_decrypt(KEY, NONCE_XIAOMI, sizeof(NONCE_XIAOMI), AAD_XIAOMI, sizeof(AAD_XIAOMI),
                                   CIPHERTEXT_XIAOMI, sizeof(CIPHERTEXT_XIAOMI), out, TAG_XIAOMI, sizeof(TAG_XIAOMI)));
  EXPECT_EQ(0, memcmp(out, PLAINTEXT_XIAOMI, sizeof(PLAINTEXT_XIAOMI)));
}

TEST(BleAesCcm, RejectsWrongAssociatedData) {
  uint8_t bad_aad[sizeof(AAD_XIAOMI)];
  memcpy(bad_aad, AAD_XIAOMI, sizeof(AAD_XIAOMI));
  bad_aad[0] ^= 0x01;
  uint8_t out[sizeof(PLAINTEXT_XIAOMI)] = {};
  EXPECT_FALSE(aes_ccm_auth_decrypt(KEY, NONCE_XIAOMI, sizeof(NONCE_XIAOMI), bad_aad, sizeof(bad_aad),
                                    CIPHERTEXT_XIAOMI, sizeof(CIPHERTEXT_XIAOMI), out, TAG_XIAOMI, sizeof(TAG_XIAOMI)));
}

TEST(BleAesCcm, DecryptsAndAuthenticatesKnownVector) {
  uint8_t out[sizeof(PLAINTEXT)] = {};
  EXPECT_TRUE(aes_ccm_auth_decrypt(KEY, NONCE, sizeof(NONCE), nullptr, 0, CIPHERTEXT, sizeof(CIPHERTEXT), out, TAG,
                                   sizeof(TAG)));
  EXPECT_EQ(0, memcmp(out, PLAINTEXT, sizeof(PLAINTEXT)));
}

TEST(BleAesCcm, RejectsTamperedTag) {
  uint8_t bad_tag[sizeof(TAG)];
  memcpy(bad_tag, TAG, sizeof(TAG));
  bad_tag[0] ^= 0x01;
  uint8_t out[sizeof(PLAINTEXT)] = {};
  EXPECT_FALSE(aes_ccm_auth_decrypt(KEY, NONCE, sizeof(NONCE), nullptr, 0, CIPHERTEXT, sizeof(CIPHERTEXT), out, bad_tag,
                                    sizeof(bad_tag)));
}

TEST(BleAesCcm, RejectsTamperedCiphertext) {
  uint8_t bad_ct[sizeof(CIPHERTEXT)];
  memcpy(bad_ct, CIPHERTEXT, sizeof(CIPHERTEXT));
  bad_ct[0] ^= 0x01;
  uint8_t out[sizeof(PLAINTEXT)] = {};
  EXPECT_FALSE(
      aes_ccm_auth_decrypt(KEY, NONCE, sizeof(NONCE), nullptr, 0, bad_ct, sizeof(bad_ct), out, TAG, sizeof(TAG)));
}

TEST(BleAesCcm, RejectsWrongKey) {
  uint8_t bad_key[sizeof(KEY)];
  memcpy(bad_key, KEY, sizeof(KEY));
  bad_key[0] ^= 0xFF;
  uint8_t out[sizeof(PLAINTEXT)] = {};
  EXPECT_FALSE(aes_ccm_auth_decrypt(bad_key, NONCE, sizeof(NONCE), nullptr, 0, CIPHERTEXT, sizeof(CIPHERTEXT), out, TAG,
                                    sizeof(TAG)));
}

}  // namespace esphome::ble_device_base::testing
