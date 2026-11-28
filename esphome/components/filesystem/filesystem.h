#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <mutex>

#include "esphome/components/storage/block_device.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome::filesystem {

enum class FsError : int8_t {
  FS_ERROR_NONE = 0,
  FS_ERROR_IO = -1,
  FS_ERROR_NO_MEDIA = -2,
  FS_ERROR_NOT_FOUND = -3,
  FS_ERROR_EXISTS = -4,
  FS_ERROR_NOT_DIR = -5,
  FS_ERROR_IS_DIR = -6,
  FS_ERROR_NO_SPACE = -7,
  FS_ERROR_TOO_MANY_OPEN = -8,
  FS_ERROR_INVALID = -9,
  FS_ERROR_NO_FILESYSTEM = -10,  // The media holds no filesystem of this type
  FS_ERROR_READ_ONLY = -11,
  FS_ERROR_DENIED = -12,
  FS_ERROR_CLOSED = -13,  // The handle is not open, or the media it was opened on is gone
  FS_ERROR_BUSY = -14,    // Called from a log callback while the filesystem mounts or formats
};

const LogString *fs_error_to_string(FsError error);

enum OpenFlag : uint8_t {
  OPEN_READ = 1 << 0,
  OPEN_WRITE = 1 << 1,
  OPEN_CREATE = 1 << 2,     // Create the file if it does not exist
  OPEN_TRUNCATE = 1 << 3,   // Empty an existing file
  OPEN_APPEND = 1 << 4,     // Start at the end of the file
  OPEN_EXCLUSIVE = 1 << 5,  // With OPEN_CREATE, fail if the file exists
};

enum class Whence : uint8_t {
  WHENCE_SET,
  WHENCE_CURRENT,
  WHENCE_END,
};

struct FileInfo {
  uint32_t size{0};
  time_t mtime{0};  // Last modification, in seconds since 1970; 0 when the filesystem does not record it
  bool is_dir{false};
};

class Filesystem;
using FsMutex = storage::NullMutex;

/// A handle to an open file. It closes the file when destroyed; it can be moved but not copied.
/// Positions and sizes are int32_t, so a file holds at most 2 GiB - 1 bytes.
class File {
 public:
  File() = default;
  File(File &&other) noexcept { this->take_(other); }
  File &operator=(File &&other) noexcept;
  File(const File &) = delete;
  File &operator=(const File &) = delete;
  ~File() { this->close(); }

  /// Read up to len bytes. Returns the number read (0 at the end of the file), or a negative FsError.
  int32_t read(void *buf, uint32_t len);
  /// Write len bytes. Returns the number written, or a negative FsError.
  int32_t write(const void *buf, uint32_t len);
  /// Move the position. Returns the new position, or a negative FsError.
  int32_t seek(int32_t offset, Whence whence = Whence::WHENCE_SET);
  int32_t tell();
  int32_t size();
  FsError sync();
  /// Cut the file at the current position.
  FsError truncate();
  FsError close();

  /// False once closed, and once the media it was opened on is unmounted or changed.
  bool is_open() const;
  explicit operator bool() const { return this->is_open(); }

 protected:
  friend class Filesystem;
  File(Filesystem *fs, uint8_t slot, uint16_t mount) : fs_(fs), slot_(slot), mount_(mount) {}
  void take_(File &other);

  Filesystem *fs_{nullptr};
  uint8_t slot_{0};
  uint16_t mount_{0};  // The mount the file was opened on
};

/// A handle to an open directory, closed when destroyed.
class Dir {
 public:
  Dir() = default;
  Dir(Dir &&other) noexcept { this->take_(other); }
  Dir &operator=(Dir &&other) noexcept;
  Dir(const Dir &) = delete;
  Dir &operator=(const Dir &) = delete;
  ~Dir() { this->close(); }

  /** Read the next entry, skipping "." and "..".
   *
   * The name is written to name, cut to fit name_size including the terminating 0.
   * @return FS_ERROR_NONE with an entry, FS_ERROR_NOT_FOUND after the last one, or another error.
   */
  FsError read_next(FileInfo &info, char *name, size_t name_size);
  FsError close();

  /// False once closed, and once the media it was opened on is unmounted or changed.
  bool is_open() const;
  explicit operator bool() const { return this->is_open(); }

 protected:
  friend class Filesystem;
  Dir(Filesystem *fs, uint8_t slot, uint16_t mount) : fs_(fs), slot_(slot), mount_(mount) {}
  void take_(Dir &other);

  Filesystem *fs_{nullptr};
  uint8_t slot_{0};
  uint16_t mount_{0};
};

/** A filesystem on a block device.
 *
 * The filesystem mounts itself in setup() when the device has media, and again on the next call after the
 * media changes. Handles opened on earlier media fail with FS_ERROR_CLOSED as soon as the media changes. A mount
 * that fails is not tried again on the same media until mount() or format() is called. File and directory handles come
 * from fixed pools sized at build time; open() fails with FS_ERROR_TOO_MANY_OPEN when the pool is empty.
 * Calls before setup(), or after setup() failed, return FS_ERROR_INVALID.
 *
 * Paths are absolute, with '/' between names. Calls block until done: on an SD card one can take hundreds of
 * milliseconds, and format() or space() several seconds on a large card.
 * Calls are made from the main loop. Each call, including those on File and Dir, holds mutex(), a NullMutex here; hold
 * it to make several calls in a row.
 */
class Filesystem : public Component {
 public:
  explicit Filesystem(storage::BlockDevice *device) : device_(device) {}

  void setup() override;
  void dump_config() override;
  /// After the block device, before the components that use the filesystem
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  /// Close everything still open, so its data and directory entries reach the media before a restart.
  void on_shutdown() override;

  /// At most 32 files and 8 directories; codegen checks the limits.
  void set_pool_sizes(uint8_t max_files, uint8_t max_dirs) {
    this->max_files_ = max_files;
    this->max_dirs_ = max_dirs;
  }
  void set_format_if_mount_failed(bool format) { this->format_if_mount_failed_ = format; }

  /// Open a file; check the result with is_open(), and last_error() for the reason it failed.
  File open(const char *path, uint8_t flags);
  Dir open_dir(const char *path);
  FsError remove(const char *path);
  FsError rename(const char *from, const char *to);
  FsError stat(const char *path, FileInfo &info);
  FsError mkdir(const char *path);
  FsError space(uint64_t &total, uint64_t &free);
  FsError format();
  /// Mount now, trying again even when mounting this media failed before.
  FsError mount();
  /// Close what is open and unmount. Nothing is written when the media has changed since it was mounted.
  /// Returns the first error from closing a file or directory.
  FsError unmount();

  bool is_mounted() const { return this->mounted_ && this->media_current_(); }
  FsMutex &mutex() { return this->mutex_; }
  FsError last_error() const { return this->last_error_; }
  storage::BlockDevice *get_device() const { return this->device_; }

 protected:
  friend class File;
  friend class Dir;

  // Driver hooks. A slot is an index below the matching pool size; the base class hands slots out. After a media
  // change fs_unmount() is called with handles still open, and must drop them without writing to the media.
  virtual FsError fs_init(uint8_t max_files, uint8_t max_dirs) = 0;
  virtual FsError fs_mount() = 0;
  virtual void fs_unmount() = 0;
  virtual FsError fs_format() = 0;
  virtual FsError fs_open(uint8_t slot, const char *path, uint8_t flags) = 0;
  virtual int32_t fs_read(uint8_t slot, void *buf, uint32_t len) = 0;
  virtual int32_t fs_write(uint8_t slot, const void *buf, uint32_t len) = 0;
  virtual int32_t fs_seek(uint8_t slot, int32_t offset, Whence whence) = 0;
  virtual int32_t fs_tell(uint8_t slot) = 0;
  virtual int32_t fs_size(uint8_t slot) = 0;
  virtual FsError fs_sync(uint8_t slot) = 0;
  virtual FsError fs_truncate(uint8_t slot) = 0;
  virtual FsError fs_close(uint8_t slot) = 0;
  virtual FsError fs_open_dir(uint8_t slot, const char *path) = 0;
  virtual FsError fs_read_dir(uint8_t slot, FileInfo &info, char *name, size_t name_size) = 0;
  virtual FsError fs_close_dir(uint8_t slot) = 0;
  virtual FsError fs_remove(const char *path) = 0;
  virtual FsError fs_rename(const char *from, const char *to) = 0;
  virtual FsError fs_stat(const char *path, FileInfo &info) = 0;
  virtual FsError fs_mkdir(const char *path) = 0;
  virtual FsError fs_space(uint64_t &total, uint64_t &free) = 0;

  /// Mount if needed, unmounting first when the media changed. Returns the error to report, or NONE.
  FsError ready_();
  FsError mount_media_();
  bool media_current_() const { return this->media_generation_ == this->device_->get_media_generation(); }
  bool file_valid_(uint16_t mount) const {
    return this->mounted_ && mount == this->mount_count_ && this->media_current_();
  }
  FsError release_file_(uint8_t slot, uint16_t mount);
  FsError release_dir_(uint8_t slot, uint16_t mount);
  FsError fail_(FsError error) {
    this->last_error_ = error;
    return error;
  }

  storage::BlockDevice *device_;
  FsMutex mutex_;
  uint32_t files_in_use_{0};      // One bit per slot
  uint16_t mount_count_{0};       // Bumped on each mount, so handles from an earlier mount are refused
  uint16_t media_generation_{0};  // The device generation that was mounted
  uint8_t dirs_in_use_{0};
  uint8_t max_files_{2};
  uint8_t max_dirs_{1};
  FsError last_error_{FsError::FS_ERROR_NONE};
  FsError mount_error_{FsError::FS_ERROR_NONE};  // Why mounting the current media failed, so it is not retried
  bool mounted_{false};
  bool format_if_mount_failed_{false};
  bool changing_{false};  // Mounting or formatting; calls from log callbacks get FS_ERROR_BUSY
};

}  // namespace esphome::filesystem
