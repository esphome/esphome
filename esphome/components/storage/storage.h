#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace storage {

enum class StorageError : uint8_t {
  OK = 0,
  NOT_READY,
  READ_ERROR,
  WRITE_ERROR,
  INVALID_ARGS,
  NOT_FOUND,
  NO_SPACE,
  PERMISSION_DENIED,
  TIMEOUT,
  CORRUPT,
};

enum class OpenMode : uint8_t {
  READ = 0,
  WRITE,
  APPEND,
  READ_WRITE,
};

struct StorageInfo {
  const char *id;
  const char *name;
  uint64_t total_bytes;
  uint64_t free_bytes;
  uint32_t block_size;
  bool is_mounted;
  bool is_removable;
  bool is_read_only;
};

#ifndef STORAGE_MAX_PATH_LEN
#define STORAGE_MAX_PATH_LEN 256
#endif

struct FileStat {
  char name[STORAGE_MAX_PATH_LEN];
  size_t size;
  bool is_dir;
};

class Storage;

struct FileHandle {
  bool in_use{false};
  const char *path{nullptr};  // Must point to driver-owned storage (e.g. char[] in driver subtype)
  Storage *storage{nullptr};
  // Drivers subclass this and add their own internals (FIL, lfs_file_t, etc.)
};

// Abstract base — all storage drivers extend one of the three subclasses below
class Storage : public Component {
 public:
  virtual StorageError get_info(StorageInfo *info) = 0;
};

// Offset-based byte access (raw flash, FRAM, EEPROM, NVS blobs)
class RawStorage : public Storage {
 public:
  virtual StorageError read(size_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred = nullptr) = 0;
  virtual StorageError write(size_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred = nullptr) = 0;
  virtual StorageError erase(size_t offset, size_t len) = 0;
  virtual StorageError format() = 0;
};

// Path-based file access with a local filesystem layer (SD, USB, LittleFS partition)
class FilesystemStorage : public Storage {
 public:
  virtual StorageError mount() = 0;
  virtual StorageError unmount() = 0;
  virtual StorageError format() = 0;
  virtual StorageError sync() = 0;
  virtual StorageError open(const char *path, FileHandle *&handle, OpenMode mode = OpenMode::READ) = 0;
  virtual StorageError close(FileHandle *handle) = 0;
  virtual StorageError read(FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred = nullptr) = 0;
  virtual StorageError write(FileHandle *handle, const uint8_t *buf, size_t len,
                             size_t *bytes_transferred = nullptr) = 0;
  virtual StorageError seek(FileHandle *handle, size_t offset) = 0;
  virtual StorageError tell(FileHandle *handle, size_t *position) = 0;
  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  virtual StorageError list_dir(const char *path, void (*callback)(const FileStat *entry, void *ctx), void *ctx) = 0;
  virtual StorageError mkdir(const char *path) = 0;
  virtual StorageError rmdir(const char *path, bool recursive = false) = 0;
  virtual StorageError remove(const char *path) = 0;
  virtual StorageError rename(const char *old_path, const char *new_path) = 0;
  virtual StorageError copy(const char *src_path, const char *dst_path) = 0;
};

// Path-based file access over a network protocol (NFS, SMB) — stateless, no file handles
class NetworkStorage : public Storage {
 public:
  virtual StorageError connect() = 0;
  virtual StorageError disconnect() = 0;
  virtual StorageError read_chunk(const char *path, uint8_t *buf, size_t offset, size_t len,
                                  size_t *bytes_transferred = nullptr) = 0;
  virtual StorageError write_chunk(const char *path, const uint8_t *buf, size_t offset, size_t len,
                                   size_t *bytes_transferred = nullptr) = 0;
  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  virtual StorageError list_dir(const char *path, void (*callback)(const FileStat *entry, void *ctx), void *ctx) = 0;
  virtual StorageError mkdir(const char *path) = 0;
  virtual StorageError rmdir(const char *path, bool recursive = false) = 0;
  virtual StorageError remove(const char *path) = 0;
  virtual StorageError rename(const char *old_path, const char *new_path) = 0;
  virtual StorageError copy(const char *src_path, const char *dst_path) = 0;
};

}  // namespace storage
}  // namespace esphome
