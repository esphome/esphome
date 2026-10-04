#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "esphome/components/api/proto.h"

namespace esphome::api::testing {

// The original byte at a time implementation.
static uint16_t reference_count(const uint8_t *data, size_t len) {
  uint16_t count = 0;
  while (len > 0) {
    while (len > 0 && (*data & 0x80)) {
      data++;
      len--;
    }
    if (len > 0) {
      data++;
      len--;
      count++;
    }
  }
  return count;
}

TEST(CountPackedVarints, EmptyBuffer) {
  const uint8_t data[1] = {0x00};
  EXPECT_EQ(count_packed_varints(data, 0), 0);
}

TEST(CountPackedVarints, SingleByteVarints) {
  const uint8_t data[] = {0x00, 0x01, 0x7F};
  EXPECT_EQ(count_packed_varints(data, sizeof(data)), 3);
}

TEST(CountPackedVarints, MultiByteVarints) {
  // 3 varints: 2 bytes, 3 bytes, 1 byte
  const uint8_t data[] = {0x80, 0x01, 0x80, 0x80, 0x01, 0x05};
  EXPECT_EQ(count_packed_varints(data, sizeof(data)), 3);
}

TEST(CountPackedVarints, TruncatedTrailingVarintIsNotCounted) {
  const uint8_t data[] = {0x05, 0x80, 0x80, 0x80};
  EXPECT_EQ(count_packed_varints(data, sizeof(data)), 1);
}

TEST(CountPackedVarints, AllContinuationBytes) {
  std::vector<uint8_t> data(5000, 0x80);
  EXPECT_EQ(count_packed_varints(data.data(), data.size()), 0);
}

TEST(CountPackedVarints, EveryStartOffsetAndLength) {
  // Cover every alignment and length around word boundaries.
  std::mt19937 rng(42);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  std::vector<uint8_t> buf(300);
  for (auto &byte : buf)
    byte = static_cast<uint8_t>(rng() & 0xFF);
  for (size_t offset = 0; offset < 16; offset++) {
    for (size_t len = 0; len + offset <= buf.size(); len++) {
      const uint16_t expected = reference_count(buf.data() + offset, len);
      EXPECT_EQ(count_packed_varints<uint32_t>(buf.data() + offset, len), expected)
          << "offset=" << offset << " len=" << len;
      EXPECT_EQ(count_packed_varints<uint64_t>(buf.data() + offset, len), expected)
          << "offset=" << offset << " len=" << len;
    }
  }
}

TEST(CountPackedVarints, LongBuffer) {
  std::mt19937 rng(7);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  std::vector<uint8_t> buf(5000);
  for (auto &byte : buf)
    byte = static_cast<uint8_t>((rng() % 4 == 0) ? (0x80 | (rng() & 0x7F)) : (rng() & 0x7F));
  for (size_t offset = 0; offset < 8; offset++) {
    const size_t len = buf.size() - offset;
    const uint16_t expected = reference_count(buf.data() + offset, len);
    EXPECT_EQ(count_packed_varints<uint32_t>(buf.data() + offset, len), expected) << "offset=" << offset;
    EXPECT_EQ(count_packed_varints<uint64_t>(buf.data() + offset, len), expected) << "offset=" << offset;
  }
}

}  // namespace esphome::api::testing
