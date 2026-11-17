#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/core/hash_base.h"

namespace esphome {

// A four byte "hash" whose digest is set directly, so the compare helpers can be tested on their own
class FixedHash : public HashBase {
 public:
  void init() override {}
  void add(const uint8_t *data, size_t len) override {}
  void calculate() override {}
  size_t get_size() const override { return 4; }
  void set_digest(const uint8_t *digest) { memcpy(this->digest_, digest, 4); }
};

static const uint8_t DIGEST[] = {0xDE, 0xAD, 0xBE, 0xEF};

TEST(HashBaseEquals, BytesMatch) {
  FixedHash hash;
  hash.set_digest(DIGEST);
  EXPECT_TRUE(hash.equals_bytes(DIGEST));
  const uint8_t other[] = {0xDE, 0xAD, 0xBE, 0xEE};
  EXPECT_FALSE(hash.equals_bytes(other));
}

TEST(HashBaseEquals, HexMatchEitherCase) {
  FixedHash hash;
  hash.set_digest(DIGEST);
  EXPECT_TRUE(hash.equals_hex("deadbeef"));
  EXPECT_TRUE(hash.equals_hex("DEADBEEF"));
  EXPECT_FALSE(hash.equals_hex("deadbeee"));
}

TEST(HashBaseEquals, HexWithoutTerminatorOnlyReadsDigestLength) {
  FixedHash hash;
  hash.set_digest(DIGEST);
  // The expected digest sits in a larger buffer with no terminator after it, as in the OTA auth buffer
  const char buffer[] = {'d', 'e', 'a', 'd', 'b', 'e', 'e', 'f', 'z', 'z', 'z', 'z'};
  EXPECT_TRUE(hash.equals_hex(buffer));
}

TEST(HashBaseEquals, ShortOrInvalidHexRejected) {
  FixedHash hash;
  hash.set_digest(DIGEST);
  EXPECT_FALSE(hash.equals_hex("deadbee"));
  EXPECT_FALSE(hash.equals_hex(""));
  EXPECT_FALSE(hash.equals_hex("deadbeeg"));
}

}  // namespace esphome
