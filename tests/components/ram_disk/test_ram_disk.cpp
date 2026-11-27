#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/ram_disk/ram_disk.h"

namespace esphome::ram_disk::testing {

using storage::StorageError;

static constexpr uint32_t BLOCK_SIZE = 512;
static constexpr uint32_t BLOCK_COUNT = 8;

TEST(RamDisk, HasNoMediaBeforeSetup) {
  RamDisk disk(BLOCK_SIZE, BLOCK_COUNT);
  uint8_t byte;
  EXPECT_FALSE(disk.has_media());
  EXPECT_EQ(disk.read(0, 0, &byte, 1), StorageError::STORAGE_ERROR_NO_MEDIA);
}

TEST(RamDisk, SetupProvidesGeometry) {
  RamDisk disk(BLOCK_SIZE, BLOCK_COUNT);
  const uint8_t generation = disk.get_media_generation();
  disk.setup();
  ASSERT_TRUE(disk.has_media());
  EXPECT_EQ(disk.get_geometry().block_size, BLOCK_SIZE);
  EXPECT_EQ(disk.get_geometry().block_count, BLOCK_COUNT);
  EXPECT_EQ(disk.get_capacity(), BLOCK_SIZE * BLOCK_COUNT);
  EXPECT_NE(disk.get_media_generation(), generation);
}

TEST(RamDisk, StartsZeroed) {
  RamDisk disk(BLOCK_SIZE, BLOCK_COUNT);
  disk.setup();
  std::array<uint8_t, BLOCK_SIZE> buf;
  buf.fill(0xAA);
  ASSERT_EQ(disk.read(BLOCK_COUNT - 1, 0, buf.data(), buf.size()), StorageError::STORAGE_ERROR_NONE);
  for (uint8_t b : buf)
    ASSERT_EQ(b, 0);
}

TEST(RamDisk, WriteReadsBackAcrossBlocks) {
  RamDisk disk(BLOCK_SIZE, BLOCK_COUNT);
  disk.setup();
  std::array<uint8_t, BLOCK_SIZE + 100> out;
  for (size_t i = 0; i < out.size(); i++)
    out[i] = static_cast<uint8_t>(i * 7 + 3);
  ASSERT_EQ(disk.program(2, BLOCK_SIZE - 50, out.data(), out.size()), StorageError::STORAGE_ERROR_NONE);

  std::array<uint8_t, BLOCK_SIZE + 100> in{};
  ASSERT_EQ(disk.read(2, BLOCK_SIZE - 50, in.data(), in.size()), StorageError::STORAGE_ERROR_NONE);
  EXPECT_EQ(in, out);

  // The bytes either side of the write are untouched
  uint8_t edge[2];
  ASSERT_EQ(disk.read(2, BLOCK_SIZE - 51, &edge[0], 1), StorageError::STORAGE_ERROR_NONE);
  ASSERT_EQ(disk.read(4, 50, &edge[1], 1), StorageError::STORAGE_ERROR_NONE);
  EXPECT_EQ(edge[0], 0);
  EXPECT_EQ(edge[1], 0);
}

TEST(RamDisk, EraseNeedsNoDriver) {
  RamDisk disk(BLOCK_SIZE, BLOCK_COUNT);
  disk.setup();
  EXPECT_EQ(disk.erase(0, BLOCK_COUNT), StorageError::STORAGE_ERROR_NONE);
  EXPECT_EQ(disk.sync(), StorageError::STORAGE_ERROR_NONE);
}

TEST(RamDisk, FailsWhenMemoryRunsOut) {
  RamDisk disk(65536, 0x40000000);  // 64 TiB
  disk.setup();
  EXPECT_TRUE(disk.is_failed());
  EXPECT_FALSE(disk.has_media());
}

}  // namespace esphome::ram_disk::testing
