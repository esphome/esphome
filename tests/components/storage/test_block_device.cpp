#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/storage/block_device.h"

namespace esphome::storage::testing {

static constexpr uint32_t BLOCK_SIZE = 4096;
static constexpr uint32_t BLOCK_COUNT = 16;

// Counts the calls that reach the driver, so a test can tell a rejected call from a forwarded one.
class FakeDevice : public BlockDevice {
 public:
  FakeDevice(uint16_t read_size, uint16_t program_size, uint8_t flags) {
    this->geometry_ = {BLOCK_SIZE, BLOCK_COUNT, read_size, program_size, flags};
  }

  void remove_media() {
    this->geometry_.block_count = 0;
    this->media_changed_();
  }

  int calls{0};

 protected:
  StorageError device_read(uint32_t block, uint32_t offset, void *buf, uint32_t len) override { return this->call_(); }
  StorageError device_program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) override {
    return this->call_();
  }
  StorageError device_erase(uint32_t block, uint32_t count) override { return this->call_(); }
  StorageError device_sync() override { return this->call_(); }
  StorageError device_trim(uint32_t block, uint32_t count) override { return this->call_(); }

  StorageError call_() {
    this->calls++;
    return StorageError::STORAGE_ERROR_NONE;
  }
};

static constexpr StorageError NONE = StorageError::STORAGE_ERROR_NONE;
static constexpr StorageError INVALID = StorageError::STORAGE_ERROR_INVALID;

TEST(BlockDevice, ForwardsAccessInRange) {
  FakeDevice dev(1, 1, 0);
  uint8_t buf[16];
  EXPECT_EQ(dev.read(3, 100, buf, sizeof(buf)), NONE);
  EXPECT_EQ(dev.program(BLOCK_COUNT - 1, BLOCK_SIZE - 16, buf, sizeof(buf)), NONE);
  EXPECT_EQ(dev.erase(0, BLOCK_COUNT), NONE);
  EXPECT_EQ(dev.sync(), NONE);
  EXPECT_EQ(dev.calls, 4);
  EXPECT_EQ(dev.get_capacity(), static_cast<uint64_t>(BLOCK_SIZE) * BLOCK_COUNT);
}

TEST(BlockDevice, AccessMaySpanBlocks) {
  FakeDevice dev(512, 512, 0);
  EXPECT_EQ(dev.read(2, 0, nullptr, 3 * BLOCK_SIZE), NONE);
  EXPECT_EQ(dev.calls, 1);
}

TEST(BlockDevice, RejectsAccessPastTheEnd) {
  FakeDevice dev(1, 1, 0);
  EXPECT_EQ(dev.read(BLOCK_COUNT, 0, nullptr, 1), INVALID);
  EXPECT_EQ(dev.read(BLOCK_COUNT - 1, 0, nullptr, BLOCK_SIZE + 1), INVALID);
  EXPECT_EQ(dev.program(BLOCK_COUNT - 1, BLOCK_SIZE, nullptr, 1), INVALID);
  // An offset near the top of the 32-bit range must not wrap around into range
  EXPECT_EQ(dev.read(0, UINT32_MAX - 7, nullptr, 16), INVALID);
  EXPECT_EQ(dev.calls, 0);
}

TEST(BlockDevice, RejectsMisalignedAccess) {
  FakeDevice dev(16, 256, 0);
  EXPECT_EQ(dev.read(0, 8, nullptr, 16), INVALID);
  EXPECT_EQ(dev.read(0, 16, nullptr, 8), INVALID);
  EXPECT_EQ(dev.program(0, 16, nullptr, 256), INVALID);
  EXPECT_EQ(dev.program(0, 256, nullptr, 100), INVALID);
  EXPECT_EQ(dev.calls, 0);
  EXPECT_EQ(dev.read(0, 16, nullptr, 32), NONE);
  EXPECT_EQ(dev.program(0, 512, nullptr, 256), NONE);
  EXPECT_EQ(dev.calls, 2);
}

TEST(BlockDevice, ZeroLengthIsNotForwarded) {
  FakeDevice dev(512, 512, 0);
  EXPECT_EQ(dev.read(0, 0, nullptr, 0), NONE);
  EXPECT_EQ(dev.program(0, 0, nullptr, 0), NONE);
  EXPECT_EQ(dev.calls, 0);
}

TEST(BlockDevice, RejectsBadEraseRange) {
  FakeDevice dev(1, 1, BLOCK_FLAG_NEEDS_ERASE);
  EXPECT_EQ(dev.erase(0, 0), INVALID);
  EXPECT_EQ(dev.erase(BLOCK_COUNT, 1), INVALID);
  EXPECT_EQ(dev.erase(BLOCK_COUNT - 1, 2), INVALID);
  EXPECT_EQ(dev.erase(1, UINT32_MAX), INVALID);
  EXPECT_EQ(dev.calls, 0);
  EXPECT_EQ(dev.erase(BLOCK_COUNT - 1, 1), NONE);
  EXPECT_EQ(dev.calls, 1);
}

TEST(BlockDevice, ReadOnlyRejectsChanges) {
  FakeDevice dev(1, 1, BLOCK_FLAG_READ_ONLY | BLOCK_FLAG_TRIM);
  static constexpr StorageError READ_ONLY = StorageError::STORAGE_ERROR_READ_ONLY;
  EXPECT_EQ(dev.program(0, 0, nullptr, 1), READ_ONLY);
  EXPECT_EQ(dev.erase(0, 1), READ_ONLY);
  EXPECT_EQ(dev.trim(0, 1), READ_ONLY);
  EXPECT_EQ(dev.calls, 0);
  EXPECT_EQ(dev.read(0, 0, nullptr, 1), NONE);
  EXPECT_EQ(dev.calls, 1);
}

TEST(BlockDevice, TrimOnlyReachesDevicesThatSupportIt) {
  FakeDevice plain(1, 1, 0);
  EXPECT_EQ(plain.trim(0, 1), NONE);
  EXPECT_EQ(plain.calls, 0);
  FakeDevice trimmable(1, 1, BLOCK_FLAG_TRIM);
  EXPECT_EQ(trimmable.trim(0, 1), NONE);
  EXPECT_EQ(trimmable.trim(0, 0), INVALID);
  EXPECT_EQ(trimmable.calls, 1);
}

TEST(BlockDevice, NoMediaRejectsEverything) {
  FakeDevice dev(1, 1, BLOCK_FLAG_REMOVABLE);
  const uint8_t generation = dev.get_media_generation();
  dev.remove_media();
  EXPECT_FALSE(dev.has_media());
  EXPECT_NE(dev.get_media_generation(), generation);
  static constexpr StorageError NO_MEDIA = StorageError::STORAGE_ERROR_NO_MEDIA;
  EXPECT_EQ(dev.read(0, 0, nullptr, 1), NO_MEDIA);
  EXPECT_EQ(dev.program(0, 0, nullptr, 1), NO_MEDIA);
  EXPECT_EQ(dev.erase(0, 1), NO_MEDIA);
  EXPECT_EQ(dev.sync(), NO_MEDIA);
  EXPECT_EQ(dev.calls, 0);
}

}  // namespace esphome::storage::testing
