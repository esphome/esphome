#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "esphome/components/filesystem/filesystem.h"
#include "esphome/components/storage/block_device.h"
#include "fake_fs.h"

namespace esphome::filesystem::testing {

// A disk in RAM whose media can be taken out and put back.
class SwapDisk : public storage::BlockDevice {
 public:
  SwapDisk(uint32_t block_size, uint32_t block_count)
      : data_(static_cast<size_t>(block_size) * block_count), block_count_(block_count) {
    this->geometry_.block_size = block_size;
  }
  void eject() {
    this->geometry_.block_count = 0;
    this->media_changed_();
  }
  void insert() {
    this->geometry_.block_count = this->block_count_;
    this->media_changed_();
  }

 protected:
  storage::StorageError device_read(uint32_t block, uint32_t offset, void *buf, uint32_t len) override {
    memcpy(buf, this->data_.data() + this->offset_(block, offset), len);
    return storage::StorageError::STORAGE_ERROR_NONE;
  }
  storage::StorageError device_program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) override {
    memcpy(this->data_.data() + this->offset_(block, offset), buf, len);
    return storage::StorageError::STORAGE_ERROR_NONE;
  }
  size_t offset_(uint32_t block, uint32_t offset) const {
    return static_cast<size_t>(block) * this->geometry_.block_size + offset;
  }

  std::vector<uint8_t> data_;
  uint32_t block_count_;
};

class FilesystemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->disk_.insert();
    this->fs_.set_pool_sizes(2, 1);
  }

  SwapDisk disk_{512, 16};
  FakeFs fs_{&disk_};
};

TEST_F(FilesystemTest, MountsInSetupWhenMediaIsThere) {
  this->fs_.call();
  EXPECT_TRUE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.mounts, 1);
}

TEST_F(FilesystemTest, MountsOnFirstUseWhenMediaArrivesLater) {
  this->disk_.eject();
  this->fs_.call();
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_FALSE(this->fs_.open("/a", OPEN_READ));
  EXPECT_EQ(this->fs_.last_error(), FsError::FS_ERROR_NO_MEDIA);
  this->disk_.insert();
  File f = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  EXPECT_TRUE(f);
  EXPECT_TRUE(this->fs_.is_mounted());
}

TEST_F(FilesystemTest, WriteThenReadBack) {
  this->fs_.call();
  {
    File f = this->fs_.open("/hello.txt", OPEN_WRITE | OPEN_CREATE);
    ASSERT_TRUE(f);
    EXPECT_EQ(f.write("hello", 5), 5);
  }
  File f = this->fs_.open("/hello.txt", OPEN_READ);
  ASSERT_TRUE(f);
  char buf[8]{};
  EXPECT_EQ(f.size(), 5);
  EXPECT_EQ(f.read(buf, sizeof(buf)), 5);
  EXPECT_STREQ(buf, "hello");
  EXPECT_EQ(f.read(buf, sizeof(buf)), 0);
}

TEST_F(FilesystemTest, OpenNeedsReadOrWrite) {
  this->fs_.call();
  EXPECT_FALSE(this->fs_.open("/a", OPEN_CREATE));
  EXPECT_EQ(this->fs_.last_error(), FsError::FS_ERROR_INVALID);
}

TEST_F(FilesystemTest, PoolRunsOutAndClosingFreesASlot) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  File b = this->fs_.open("/b", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_FALSE(this->fs_.open("/c", OPEN_WRITE | OPEN_CREATE));
  EXPECT_EQ(this->fs_.last_error(), FsError::FS_ERROR_TOO_MANY_OPEN);
  a.close();
  EXPECT_FALSE(a);
  EXPECT_TRUE(this->fs_.open("/c", OPEN_WRITE | OPEN_CREATE));
}

TEST_F(FilesystemTest, MovingAHandleKeepsOneOwner) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  File b = std::move(a);
  EXPECT_FALSE(a);  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(b);
  b = File();
  EXPECT_EQ(this->fs_.closes, 1);
  // Both slots are free again
  File c = this->fs_.open("/c", OPEN_WRITE | OPEN_CREATE);
  File d = this->fs_.open("/d", OPEN_WRITE | OPEN_CREATE);
  EXPECT_TRUE(c);
  EXPECT_TRUE(d);
}

TEST_F(FilesystemTest, MediaChangeClosesOldHandles) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  File b = this->fs_.open("/b", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(a);
  this->disk_.eject();
  this->disk_.insert();
  // The next call notices the change, closes everything and mounts the new media
  File c = this->fs_.open("/c", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(c);
  EXPECT_EQ(this->fs_.mounts, 2);
  EXPECT_EQ(a.write("x", 1), static_cast<int32_t>(FsError::FS_ERROR_CLOSED));
  // A stale handle closing must not free the slot now used by c
  EXPECT_EQ(a.close(), FsError::FS_ERROR_CLOSED);
  File d = this->fs_.open("/d", OPEN_WRITE | OPEN_CREATE);
  EXPECT_TRUE(d);
  EXPECT_FALSE(this->fs_.open("/e", OPEN_WRITE | OPEN_CREATE));
  EXPECT_EQ(c.write("y", 1), 1);
}

TEST_F(FilesystemTest, UnmountClosesOpenFiles) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  this->fs_.unmount();
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.closes, 1);
  EXPECT_EQ(a.sync(), FsError::FS_ERROR_CLOSED);
}

TEST_F(FilesystemTest, ShutdownClosesOpenFiles) {
  this->fs_.call();
  File f = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(f);
  this->fs_.on_shutdown();
  EXPECT_EQ(this->fs_.closes, 1);
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_EQ(f.write("x", 1), static_cast<int32_t>(FsError::FS_ERROR_CLOSED));
}

TEST_F(FilesystemTest, FormatsOnlyWhenAsked) {
  this->fs_.has_filesystem = false;
  this->fs_.call();
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.formats, 0);

  FakeFs formatting{&this->disk_};
  formatting.has_filesystem = false;
  formatting.set_format_if_mount_failed(true);
  formatting.call();
  EXPECT_TRUE(formatting.is_mounted());
  EXPECT_EQ(formatting.formats, 1);
}

TEST_F(FilesystemTest, StaleHandleRefusedBeforeAnyOtherCall) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(a);
  this->disk_.eject();
  this->disk_.insert();
  // Nothing has told the filesystem yet; the handle must still not reach the new media
  EXPECT_EQ(a.write("x", 1), static_cast<int32_t>(FsError::FS_ERROR_CLOSED));
  EXPECT_EQ(a.sync(), FsError::FS_ERROR_CLOSED);
  EXPECT_EQ(this->fs_.files["/a"].size(), 0u);
}

TEST_F(FilesystemTest, MediaChangeDropsHandlesWithoutWriting) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  Dir d = this->fs_.open_dir("/");
  this->disk_.eject();
  this->disk_.insert();
  File b = this->fs_.open("/b", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(b);
  EXPECT_EQ(this->fs_.closes, 0);
  // unmount() on the same media still closes, so buffered data is written
  this->fs_.unmount();
  EXPECT_EQ(this->fs_.closes, 1);
}

TEST_F(FilesystemTest, UnmountAfterMediaChangeWritesNothing) {
  this->fs_.call();
  File a = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  this->disk_.eject();
  this->fs_.unmount();
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.closes, 0);
}

TEST_F(FilesystemTest, FailedMountNotRetriedOnSameMedia) {
  this->fs_.has_filesystem = false;
  this->fs_.call();
  EXPECT_EQ(this->fs_.mounts, 1);
  FileInfo info;
  EXPECT_EQ(this->fs_.stat("/a", info), FsError::FS_ERROR_NO_FILESYSTEM);
  EXPECT_FALSE(this->fs_.open("/a", OPEN_READ));
  EXPECT_EQ(this->fs_.last_error(), FsError::FS_ERROR_NO_FILESYSTEM);
  EXPECT_EQ(this->fs_.mounts, 1);
  // mount() asks again
  EXPECT_EQ(this->fs_.mount(), FsError::FS_ERROR_NO_FILESYSTEM);
  EXPECT_EQ(this->fs_.mounts, 2);
  // So does new media
  this->fs_.has_filesystem = true;
  this->disk_.eject();
  this->disk_.insert();
  EXPECT_TRUE(this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE));
  EXPECT_EQ(this->fs_.mounts, 3);
}

TEST_F(FilesystemTest, FormatClearsFailedMount) {
  this->fs_.has_filesystem = false;
  this->fs_.call();
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.format(), FsError::FS_ERROR_NONE);
  EXPECT_TRUE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.mounts, 2);
}

TEST_F(FilesystemTest, ListsDirectory) {
  this->fs_.call();
  this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE).write("12", 2);
  this->fs_.open("/b", OPEN_WRITE | OPEN_CREATE);
  Dir dir = this->fs_.open_dir("/");
  ASSERT_TRUE(dir);
  FileInfo info;
  char name[4];
  ASSERT_EQ(dir.read_next(info, name, sizeof(name)), FsError::FS_ERROR_NONE);
  EXPECT_STREQ(name, "/a");
  EXPECT_EQ(info.size, 2u);
  ASSERT_EQ(dir.read_next(info, name, sizeof(name)), FsError::FS_ERROR_NONE);
  EXPECT_STREQ(name, "/b");
  EXPECT_EQ(dir.read_next(info, name, sizeof(name)), FsError::FS_ERROR_NOT_FOUND);
  EXPECT_FALSE(this->fs_.open_dir("/"));
  EXPECT_EQ(this->fs_.last_error(), FsError::FS_ERROR_TOO_MANY_OPEN);
}

TEST_F(FilesystemTest, PathCallsReportErrors) {
  this->fs_.call();
  FileInfo info;
  EXPECT_EQ(this->fs_.stat("/missing", info), FsError::FS_ERROR_NOT_FOUND);
  EXPECT_EQ(this->fs_.remove("/missing"), FsError::FS_ERROR_NOT_FOUND);
  EXPECT_EQ(this->fs_.stat(nullptr, info), FsError::FS_ERROR_INVALID);
  this->disk_.eject();
  EXPECT_EQ(this->fs_.mkdir("/d"), FsError::FS_ERROR_NO_MEDIA);
}

TEST_F(FilesystemTest, CallsBeforeSetupDoNotMount) {
  EXPECT_FALSE(this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE));
  EXPECT_EQ(this->fs_.last_error(), FsError::FS_ERROR_INVALID);
  EXPECT_EQ(this->fs_.format(), FsError::FS_ERROR_INVALID);
  EXPECT_EQ(this->fs_.mounts, 0);
  EXPECT_EQ(this->fs_.formats, 0);
}

TEST_F(FilesystemTest, FailedInitDoesNotMount) {
  this->fs_.init_error = FsError::FS_ERROR_IO;
  this->fs_.call();
  EXPECT_TRUE(this->fs_.is_failed());
  EXPECT_FALSE(this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE));
  EXPECT_EQ(this->fs_.mount(), FsError::FS_ERROR_INVALID);
  EXPECT_EQ(this->fs_.format(), FsError::FS_ERROR_INVALID);
  EXPECT_EQ(this->fs_.mounts, 0);
  EXPECT_EQ(this->fs_.formats, 0);
}

TEST_F(FilesystemTest, CallsDuringFormatAreBusy) {
  this->fs_.call();
  FsError inner = FsError::FS_ERROR_NONE;
  this->fs_.on_format = [this, &inner]() {
    File f = this->fs_.open("/log", OPEN_WRITE | OPEN_CREATE);
    inner = this->fs_.last_error();
    EXPECT_FALSE(f);
    EXPECT_EQ(this->fs_.format(), FsError::FS_ERROR_BUSY);
  };
  EXPECT_EQ(this->fs_.format(), FsError::FS_ERROR_NONE);
  EXPECT_EQ(inner, FsError::FS_ERROR_BUSY);
  EXPECT_EQ(this->fs_.formats, 1);
  EXPECT_TRUE(this->fs_.is_mounted());
}

TEST_F(FilesystemTest, CallsDuringFormatOnMountAreBusy) {
  this->fs_.has_filesystem = false;
  this->fs_.set_format_if_mount_failed(true);
  FsError inner = FsError::FS_ERROR_NONE;
  this->fs_.on_format = [this, &inner]() {
    File f = this->fs_.open("/log", OPEN_WRITE | OPEN_CREATE);
    inner = this->fs_.last_error();
  };
  this->fs_.call();
  EXPECT_EQ(inner, FsError::FS_ERROR_BUSY);
  EXPECT_EQ(this->fs_.formats, 1);
  EXPECT_TRUE(this->fs_.is_mounted());
}

TEST_F(FilesystemTest, StaleHandleIsNotOpen) {
  this->fs_.call();
  File f = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  Dir d = this->fs_.open_dir("/");
  ASSERT_TRUE(f);
  ASSERT_TRUE(d);
  this->disk_.eject();
  EXPECT_FALSE(f);
  EXPECT_FALSE(d);
  EXPECT_FALSE(this->fs_.is_mounted());
}

TEST_F(FilesystemTest, StaleHandleRefusedAfterManyMediaChanges) {
  this->fs_.call();
  File f = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  ASSERT_TRUE(f);
  // 256 generation steps, with no filesystem call to notice them
  for (int i = 0; i != 128; i++) {
    this->disk_.eject();
    this->disk_.insert();
  }
  EXPECT_FALSE(f);
  EXPECT_EQ(f.write("x", 1), static_cast<int32_t>(FsError::FS_ERROR_CLOSED));
}

TEST_F(FilesystemTest, UnmountReportsCloseError) {
  this->fs_.call();
  File f = this->fs_.open("/a", OPEN_WRITE | OPEN_CREATE);
  this->fs_.close_error = FsError::FS_ERROR_IO;
  EXPECT_EQ(this->fs_.unmount(), FsError::FS_ERROR_IO);
  EXPECT_FALSE(this->fs_.is_mounted());
  EXPECT_EQ(this->fs_.unmount(), FsError::FS_ERROR_NONE);
}

TEST(Filesystem, ErrorNames) {
  EXPECT_STREQ(LOG_STR_ARG(fs_error_to_string(FsError::FS_ERROR_NONE)), "none");
  EXPECT_STREQ(LOG_STR_ARG(fs_error_to_string(FsError::FS_ERROR_NO_SPACE)), "no space");
  EXPECT_STREQ(LOG_STR_ARG(fs_error_to_string(FsError::FS_ERROR_CLOSED)), "closed");
  EXPECT_STREQ(LOG_STR_ARG(fs_error_to_string(FsError::FS_ERROR_BUSY)), "busy");
  EXPECT_STREQ(LOG_STR_ARG(fs_error_to_string(static_cast<FsError>(1))), "unknown");
}

}  // namespace esphome::filesystem::testing
