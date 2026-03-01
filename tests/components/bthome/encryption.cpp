#include <gtest/gtest.h>
#include "esphome/components/bthome/encryption.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome::bthome::testing {

// Vector wrapper for bthome_encrypt.
// Returns encrypted data (ciphertext + counter + mic), or empty on failure.
static std::vector<uint8_t> encrypt(const std::vector<uint8_t> &plaintext, MacAddressPtr source_address,
                                    uint32_t counter, BTHomeHeader header, const EncryptionKey &key) {
  size_t encrypted_size = 0;
  const uint8_t *result =
      bthome_encrypt(plaintext.data(), plaintext.size(), source_address, counter, header, key, encrypted_size);
  if (result == nullptr)
    return {};
  return {result, result + encrypted_size};
}

// Vector wrapper for bthome_decrypt.
// Returns decrypted plaintext, or empty on failure.
static std::vector<uint8_t> decrypt(const std::vector<uint8_t> &ciphertext, MacAddressPtr source_address,
                                    BTHomeHeader header, const EncryptionKey &key) {
  size_t plaintext_size = 0;
  const uint8_t *result =
      bthome_decrypt(ciphertext.data(), ciphertext.size(), source_address, header, key, plaintext_size);
  if (result == nullptr)
    return {};
  return {result, result + plaintext_size};
}

class BTHomeEncryptionTest : public ::testing::Test {
 protected:
  // Test MAC address: 54:48:E6:8F:80:A5
  MacAddress test_mac_{0x5448E68F80A5ULL};

  // Test encryption key
  EncryptionKey test_key_{0x23, 0x1d, 0x39, 0xc1, 0xd7, 0xcc, 0x1a, 0xb1,
                          0xae, 0xe2, 0x24, 0xcd, 0x09, 0x6d, 0xb9, 0x32};

  // Different key for negative tests
  EncryptionKey wrong_key_{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

  BTHomeHeader header_{.encrypted = 1, .trigger_based = 0, .version = 2};
};

// Test: Encrypt and decrypt simple plaintext
TEST_F(BTHomeEncryptionTest, EncryptDecryptRoundTrip) {
  std::vector<uint8_t> plaintext = {0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13};
  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 12345, header_, test_key_);
  ASSERT_FALSE(encrypted.empty()) << "Encryption failed";
  EXPECT_EQ(encrypted.size(), plaintext.size() + 4 + 4);

  auto decrypted = decrypt(encrypted, source_addr, header_, test_key_);
  ASSERT_FALSE(decrypted.empty()) << "Decryption failed";
  EXPECT_EQ(decrypted, plaintext);
}

// Test: Encrypt and decrypt with different counter values
TEST_F(BTHomeEncryptionTest, EncryptDecryptDifferentCounters) {
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  MacAddressPtr source_addr(test_mac_);

  for (uint32_t counter = 0; counter < 1000; counter += 100) {
    auto encrypted = encrypt(plaintext, source_addr, counter, header_, test_key_);
    ASSERT_FALSE(encrypted.empty()) << "Encryption failed for counter " << counter;

    auto decrypted = decrypt(encrypted, source_addr, header_, test_key_);
    ASSERT_FALSE(decrypted.empty()) << "Decryption failed for counter " << counter;
    EXPECT_EQ(decrypted, plaintext) << "Mismatch at counter " << counter;
  }
}

// Test: Decrypt with wrong key fails
TEST_F(BTHomeEncryptionTest, DecryptWithWrongKeyFails) {
  std::vector<uint8_t> plaintext = {0xAA, 0xBB, 0xCC, 0xDD};
  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 999, header_, test_key_);
  ASSERT_FALSE(encrypted.empty());

  auto decrypted = decrypt(encrypted, source_addr, header_, wrong_key_);
  EXPECT_TRUE(decrypted.empty()) << "Decryption should fail with wrong key";
}

// Test: Decrypt with corrupted ciphertext fails
TEST_F(BTHomeEncryptionTest, DecryptWithCorruptedCiphertextFails) {
  std::vector<uint8_t> plaintext = {0x11, 0x22, 0x33, 0x44};
  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 555, header_, test_key_);
  ASSERT_FALSE(encrypted.empty());

  encrypted[1] ^= 0xFF;

  auto decrypted = decrypt(encrypted, source_addr, header_, test_key_);
  EXPECT_TRUE(decrypted.empty()) << "Decryption should fail with corrupted ciphertext";
}

// Test: Decrypt with corrupted MIC fails
TEST_F(BTHomeEncryptionTest, DecryptWithCorruptedMicFails) {
  std::vector<uint8_t> plaintext = {0xFF, 0xEE, 0xDD, 0xCC};
  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 777, header_, test_key_);
  ASSERT_FALSE(encrypted.empty());

  encrypted.back() ^= 0xFF;

  auto decrypted = decrypt(encrypted, source_addr, header_, test_key_);
  EXPECT_TRUE(decrypted.empty()) << "Decryption should fail with corrupted MIC";
}

// Test: Decrypt with too short ciphertext fails
TEST_F(BTHomeEncryptionTest, DecryptWithTooShortCiphertextFails) {
  MacAddressPtr source_addr(test_mac_);

  auto decrypted = decrypt({0x01, 0x02}, source_addr, header_, test_key_);
  EXPECT_TRUE(decrypted.empty()) << "Decryption should fail with too short ciphertext";
}

// Test: Encrypt with maximum plaintext size
TEST_F(BTHomeEncryptionTest, EncryptMaximumPlaintextSize) {
  // Maximum plaintext: 31 - 3 (ble flags) - 4 (adv header) - 1 (BTHome Header) - 4 (counter) - 4 (mic) = 15 bytes
  std::vector<uint8_t> plaintext(15);
  for (size_t i = 0; i < plaintext.size(); i++)
    plaintext[i] = i & 0xFF;

  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 1000, header_, test_key_);
  ASSERT_FALSE(encrypted.empty()) << "Encryption should succeed with maximum plaintext size";
  EXPECT_EQ(encrypted.size(), 15 + 4 + 4) << "Encrypted size should be plaintext size + counter + mic";
}

// Test: Encrypt with plaintext larger than buffer fails
TEST_F(BTHomeEncryptionTest, EncryptWithPlaintextTooLargeFails) {
  // 17 bytes would result in 25 bytes encrypted, exceeding the 23-byte buffer
  std::vector<uint8_t> plaintext(17);
  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 2000, header_, test_key_);
  EXPECT_TRUE(encrypted.empty()) << "Encryption should fail with plaintext too large";
}

// Test: Encrypt and decrypt with single-byte plaintext
TEST_F(BTHomeEncryptionTest, EncryptDecryptSingleByte) {
  std::vector<uint8_t> plaintext = {0x42};
  MacAddressPtr source_addr(test_mac_);

  auto encrypted = encrypt(plaintext, source_addr, 111, header_, test_key_);
  ASSERT_FALSE(encrypted.empty());

  auto decrypted = decrypt(encrypted, source_addr, header_, test_key_);
  ASSERT_FALSE(decrypted.empty());
  EXPECT_EQ(decrypted, plaintext);
}

// Test: Different MACs produce different ciphertexts
TEST_F(BTHomeEncryptionTest, DifferentMacProducesDifferentCiphertext) {
  std::vector<uint8_t> plaintext = {0x55, 0x66, 0x77, 0x88};

  MacAddress different_mac{0xAABBCCDDEEFFULL};
  MacAddressPtr source_addr1(test_mac_);
  MacAddressPtr source_addr2(different_mac);

  auto encrypted1 = encrypt(plaintext, source_addr1, 333, header_, test_key_);
  auto encrypted2 = encrypt(plaintext, source_addr2, 333, header_, test_key_);

  ASSERT_FALSE(encrypted1.empty());
  ASSERT_FALSE(encrypted2.empty());
  EXPECT_NE(encrypted1, encrypted2) << "Different MACs should produce different ciphertexts";
}

// Test: Decrypt with different MAC fails
TEST_F(BTHomeEncryptionTest, DecryptWithDifferentMacFails) {
  std::vector<uint8_t> plaintext = {0x12, 0x34, 0x56, 0x78};

  MacAddress different_mac{0x112233445566ULL};
  MacAddressPtr source_addr1(test_mac_);
  MacAddressPtr source_addr2(different_mac);

  auto encrypted = encrypt(plaintext, source_addr1, 444, header_, test_key_);
  ASSERT_FALSE(encrypted.empty());

  auto decrypted = decrypt(encrypted, source_addr2, header_, test_key_);
  EXPECT_TRUE(decrypted.empty()) << "Decryption should fail with different MAC";
}

}  // namespace esphome::bthome::testing
