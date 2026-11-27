// Pins the lazy erase-ahead arithmetic used by the ESP-IDF OTA backend: the
// erased watermark must always cover the write end, stay 64 KiB block-aligned
// until the clamp, and never exceed the partition.

#include <gtest/gtest.h>

#include "esphome/components/ota/ota_backend.h"

namespace esphome::ota::testing {

static constexpr size_t BLOCK = 64 * 1024;
static constexpr size_t PART = 1835008;  // 0x1C0000, a real app slot size

TEST(NextEraseEnd, FirstWriteRoundsUpToOneBlock) { EXPECT_EQ(next_erase_end(1024, PART), BLOCK); }

TEST(NextEraseEnd, ExactBlockBoundaryDoesNotOverErase) { EXPECT_EQ(next_erase_end(BLOCK, PART), BLOCK); }

TEST(NextEraseEnd, StraddlingWriteCoversNextBlock) { EXPECT_EQ(next_erase_end(BLOCK + 1, PART), 2 * BLOCK); }

TEST(NextEraseEnd, ClampsToPartitionEnd) {
  // Partition sizes are sector multiples but not always block multiples
  constexpr size_t part = 27 * BLOCK + 4096;
  EXPECT_EQ(next_erase_end(27 * BLOCK + 1, part), part);
  EXPECT_EQ(next_erase_end(part, part), part);
}

// Bootloader staging seeds erased_end_ mid-block (e.g. 0x8000); the target for
// a write past that seed must still cover the write end.
TEST(NextEraseEnd, MidBlockSeedStillCovered) { EXPECT_EQ(next_erase_end(0x8000 + 1024, PART), BLOCK); }

TEST(NextEraseEnd, SweepAlwaysCoversWriteEndWithinPartition) {
  for (size_t end = 1; end <= PART; end += 4093) {
    const size_t erased = next_erase_end(end, PART);
    ASSERT_GE(erased, end);
    ASSERT_LE(erased, PART);
    // Block-aligned unless clamped at the partition end
    ASSERT_TRUE(erased == PART || erased % BLOCK == 0);
  }
}

}  // namespace esphome::ota::testing
