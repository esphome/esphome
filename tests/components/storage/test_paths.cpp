#include <gtest/gtest.h>

#include "esphome/components/storage/storage.h"

// Covers the parts of the storage interface that are pure logic: path resolution and joining,
// the tree walks' path building, and the error-name table. These are what the YAML tests
// cannot reach -- a compile test never calls them, and several of the defects found in review
// (a mapper missing cases, a walk that left a stale path behind on overflow, a prefix match
// that could shadow a sibling mount) were invisible to them for exactly that reason.

namespace esphome::storage::testing {

// Minimal PathStorage: the registry only ever asks these for their type and mount path, so the
// data-plane methods just have to exist.
class DummyPathStorage : public FilesystemStorage {
 public:
  explicit DummyPathStorage(const char *mount) { this->set_mount_path_(mount); }

  StorageError get_info(StorageInfo *info) override {
    *info = StorageInfo{};
    return StorageError::OK;
  }
  StorageError stat(const char *, FileStat *) override { return StorageError::NOT_FOUND; }
  StorageError list_dir(const char *, bool (*)(const FileStat *, void *), void *) override {
    return StorageError::NOT_SUPPORTED;
  }
  StorageError mkdir(const char *) override { return StorageError::NOT_SUPPORTED; }
  StorageError rmdir(const char *) override { return StorageError::NOT_SUPPORTED; }
  StorageError remove(const char *) override { return StorageError::NOT_SUPPORTED; }
  StorageError rename(const char *, const char *) override { return StorageError::NOT_SUPPORTED; }
  StorageError mount() override { return StorageError::OK; }
  StorageError unmount() override { return StorageError::OK; }
  StorageError format() override { return StorageError::NOT_SUPPORTED; }
  StorageError sync() override { return StorageError::OK; }
  StorageError open(const char *, FileHandle *&, OpenMode) override { return StorageError::NOT_SUPPORTED; }
  StorageError close(FileHandle *) override { return StorageError::OK; }
  StorageError read(FileHandle *, uint8_t *, size_t, size_t *) override { return StorageError::NOT_SUPPORTED; }
  StorageError write(FileHandle *, const uint8_t *, size_t, size_t *) override { return StorageError::NOT_SUPPORTED; }
  StorageError seek(FileHandle *, int64_t, SeekMode) override { return StorageError::NOT_SUPPORTED; }
  StorageError tell(FileHandle *, uint64_t *) override { return StorageError::NOT_SUPPORTED; }
};

class RegistryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->registry_.set_device_count(3);
    ASSERT_EQ(this->registry_.register_storage(&this->sd_), StorageError::OK);
    ASSERT_EQ(this->registry_.register_storage(&this->sd_nested_), StorageError::OK);
    ASSERT_EQ(this->registry_.register_storage(&this->usb_), StorageError::OK);
  }

  StorageRegistry registry_;
  DummyPathStorage sd_{"/sd"};
  DummyPathStorage sd_nested_{"/sd/nested"};
  DummyPathStorage usb_{"/usb"};
};

TEST_F(RegistryTest, ResolvePathSplitsAtTheMountPoint) {
  const char *rel = nullptr;
  EXPECT_EQ(this->registry_.resolve_path("/sd/dir/file.txt", &rel), &this->sd_);
  EXPECT_STREQ(rel, "/dir/file.txt");
}

TEST_F(RegistryTest, ResolvePathReturnsEmptyRelForTheMountPointItself) {
  const char *rel = nullptr;
  EXPECT_EQ(this->registry_.resolve_path("/sd", &rel), &this->sd_);
  EXPECT_STREQ(rel, "");
}

TEST_F(RegistryTest, ResolvePathMatchesOnlyAtASeparator) {
  // "/sd2" shares a prefix with "/sd" but is a different mount point, so nothing must match.
  const char *rel = nullptr;
  EXPECT_EQ(this->registry_.resolve_path("/sd2/file.txt", &rel), nullptr);
}

TEST_F(RegistryTest, ResolvePathPrefersTheLongestMountPoint) {
  // validate_mount_path() rejects nested mount points, so this cannot come from YAML any
  // more. The longest-prefix rule stays as defence and is kept covered here.
  const char *rel = nullptr;
  EXPECT_EQ(this->registry_.resolve_path("/sd/nested/file.txt", &rel), &this->sd_nested_);
  EXPECT_STREQ(rel, "/file.txt");
}

TEST_F(RegistryTest, ResolvePathLeavesRelUntouchedWhenNothingMatches) {
  const char *rel = "sentinel";
  EXPECT_EQ(this->registry_.resolve_path("/nowhere/file.txt", &rel), nullptr);
  EXPECT_STREQ(rel, "sentinel");
}

TEST_F(RegistryTest, GetRefusesAnIndexPastTheEnd) {
  ASSERT_EQ(this->registry_.size(), 3u);
  EXPECT_NE(this->registry_.get(0), nullptr);
  EXPECT_EQ(this->registry_.get(3), nullptr);
  EXPECT_EQ(this->registry_.get(SIZE_MAX), nullptr);
}

TEST(BuildPath, JoinsWithExactlyOneSeparator) {
  DummyPathStorage sd{"/sd"};
  char out[64];
  ASSERT_TRUE(StorageRegistry::build_path(&sd, "dir/file.txt", out, sizeof(out)));
  EXPECT_STREQ(out, "/sd/dir/file.txt");
  // A leading separator on the relative part must not produce a doubled one.
  ASSERT_TRUE(StorageRegistry::build_path(&sd, "/dir/file.txt", out, sizeof(out)));
  EXPECT_STREQ(out, "/sd/dir/file.txt");
}

TEST(BuildPath, YieldsTheMountPointForAnEmptyRelative) {
  DummyPathStorage sd{"/sd"};
  char out[64];
  ASSERT_TRUE(StorageRegistry::build_path(&sd, "", out, sizeof(out)));
  EXPECT_STREQ(out, "/sd");
}

TEST(BuildPath, TreatsASoleSlashAsTheMountPointItself) {
  // resolve_path() reports "" for a path that IS the mount point; "/" means the same thing and
  // must not leave a trailing separator behind.
  DummyPathStorage sd{"/sd"};
  char out[64];
  ASSERT_TRUE(StorageRegistry::build_path(&sd, "/", out, sizeof(out)));
  EXPECT_STREQ(out, "/sd");
}

TEST(BuildPath, RefusesWhatWouldNotFit) {
  DummyPathStorage sd{"/sd"};
  char out[8];
  EXPECT_FALSE(StorageRegistry::build_path(&sd, "much/too/long", out, sizeof(out)));
}

TEST(ErrorToString, NamesEveryEnumerator) {
  // "UNKNOWN" is the fallthrough: every enumerator must have its own case, or a new one added
  // later renders as UNKNOWN in every log line that reports it.
  const StorageError all[] = {
      StorageError::OK,
      StorageError::NOT_FOUND,
      StorageError::READ_ERROR,
      StorageError::WRITE_ERROR,
      StorageError::INVALID_ARGS,
      StorageError::NO_SPACE,
      StorageError::NOT_READY,
      StorageError::PERMISSION_DENIED,
      StorageError::TIMEOUT,
      StorageError::CORRUPT,
      StorageError::NOT_SUPPORTED,
      StorageError::ALREADY_EXISTS,
      StorageError::NOT_EMPTY,
      StorageError::TOO_MANY_OPEN_FILES,
      StorageError::TRANSFER_TOO_LARGE,
      StorageError::VERIFY_MISMATCH,
  };
  for (StorageError err : all)
    EXPECT_STRNE(error_to_string(err), "UNKNOWN");
}

TEST(ErrorFromErrno, MapsEveryErrnoTheEnumClaims) {
  EXPECT_EQ(error_from_errno(ENOENT, false), StorageError::NOT_FOUND);
  EXPECT_EQ(error_from_errno(EEXIST, false), StorageError::ALREADY_EXISTS);
  EXPECT_EQ(error_from_errno(ENOTEMPTY, false), StorageError::NOT_EMPTY);
  EXPECT_EQ(error_from_errno(ENOSPC, true), StorageError::NO_SPACE);
  EXPECT_EQ(error_from_errno(EACCES, false), StorageError::PERMISSION_DENIED);
  EXPECT_EQ(error_from_errno(EMFILE, false), StorageError::TOO_MANY_OPEN_FILES);
  EXPECT_EQ(error_from_errno(EINVAL, false), StorageError::INVALID_ARGS);
  EXPECT_EQ(error_from_errno(ENOTSUP, false), StorageError::NOT_SUPPORTED);
  EXPECT_EQ(error_from_errno(ENODEV, false), StorageError::NOT_READY);
  EXPECT_EQ(error_from_errno(ETIMEDOUT, false), StorageError::TIMEOUT);
  EXPECT_EQ(error_from_errno(EILSEQ, false), StorageError::CORRUPT);
}

TEST(ErrorFromErrno, FallbackFollowsTheDirection) {
  // EIO has no dedicated case; `writing` decides which way an unmapped errno is reported.
  EXPECT_EQ(error_from_errno(EIO, false), StorageError::READ_ERROR);
  EXPECT_EQ(error_from_errno(EIO, true), StorageError::WRITE_ERROR);
}

}  // namespace esphome::storage::testing
