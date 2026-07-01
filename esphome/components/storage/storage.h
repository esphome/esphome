#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <cstdio>

#ifdef USE_ESP_IDF

namespace esphome::storage {

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

// Identifies the concrete subtype of a Storage pointer without RTTI.
// Used by StorageRegistry::for_each_filesystem/raw/network() to safely
// static_cast without dynamic_cast (RTTI is disabled on ESP32).
enum class StorageType : uint8_t {
  RAW = 0,
  FILESYSTEM,
  NETWORK,
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

struct FileStat {
  char name[CONFIG_FATFS_MAX_LFN + 1];
  size_t size;
  bool is_dir;
};

class Storage;

struct FileHandle {
  bool in_use{false};
  const char *path{nullptr};  // Must point to driver-owned storage (e.g. char[] in driver subtype)
  Storage *storage{nullptr};
  FILE *file{nullptr};  // POSIX handle — valid for all VFS-backed drivers, nullptr otherwise
  // Drivers bypassing VFS subclass this and add their own handle (lfs_file_t, etc.)
};

// Abstract base — all storage drivers extend one of the three subclasses below
class Storage : public Component {
 public:
  virtual StorageError get_info(StorageInfo *info) = 0;
  virtual StorageType get_storage_type() const = 0;
};

// Offset-based byte access (raw flash, FRAM, EEPROM, NVS blobs)
class RawStorage : public Storage {
 public:
  StorageType get_storage_type() const override { return StorageType::RAW; }

  virtual StorageError read(size_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError write(size_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError erase(size_t offset, size_t len) = 0;
  virtual StorageError format() = 0;
};

// Path-based file access with a local filesystem layer (SD, USB, LittleFS partition)
class FilesystemStorage : public Storage {
 public:
  StorageType get_storage_type() const override { return StorageType::FILESYSTEM; }

  virtual StorageError mount() = 0;
  virtual StorageError unmount() = 0;
  virtual StorageError format() = 0;
  virtual StorageError sync() = 0;
  virtual StorageError open(const char *path, FileHandle *&handle, OpenMode mode) = 0;
  virtual StorageError close(FileHandle *handle) = 0;
  virtual StorageError read(FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError write(FileHandle *handle, const uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError seek(FileHandle *handle, size_t offset) = 0;
  virtual StorageError tell(FileHandle *handle, size_t *position) = 0;
  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  virtual StorageError list_dir(const char *path, void (*callback)(const FileStat *entry, void *ctx), void *ctx) = 0;
  virtual StorageError mkdir(const char *path) = 0;
  virtual StorageError rmdir(const char *path, bool recursive) = 0;
  virtual StorageError remove(const char *path) = 0;
  virtual StorageError rename(const char *old_path, const char *new_path) = 0;
  virtual StorageError copy(const char *src_path, const char *dst_path) = 0;
};

// Path-based file access over a network protocol (NFS, SMB) — stateless, no file handles
class NetworkStorage : public Storage {
 public:
  StorageType get_storage_type() const override { return StorageType::NETWORK; }

  virtual StorageError connect() = 0;
  virtual StorageError disconnect() = 0;
  virtual StorageError read_chunk(const char *path, uint8_t *buf, size_t offset, size_t len,
                                  size_t *bytes_transferred) = 0;
  virtual StorageError write_chunk(const char *path, const uint8_t *buf, size_t offset, size_t len,
                                   size_t *bytes_transferred) = 0;
  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  virtual StorageError list_dir(const char *path, void (*callback)(const FileStat *entry, void *ctx), void *ctx) = 0;
  virtual StorageError mkdir(const char *path) = 0;
  virtual StorageError rmdir(const char *path, bool recursive) = 0;
  virtual StorageError remove(const char *path) = 0;
  virtual StorageError rename(const char *old_path, const char *new_path) = 0;
  virtual StorageError copy(const char *src_path, const char *dst_path) = 0;
};

// Runtime registry of all storage devices.
// Initializes before any driver (setup_priority::BUS) so drivers can safely
// call register_storage() / unregister_storage() from their own setup() or
// on hotplug events. Pool is sized exactly at codegen time from the number of
// configured devices — no compile-time upper bound, no wasted slots.
class StorageRegistry : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }

  // Called by codegen with the exact number of configured storage devices
  void set_device_count(size_t count) { this->storages_.init(count); }

  void register_storage(Storage *s);
  void unregister_storage(Storage *s);

  // Enumerate by type — callback receives each matching device and caller ctx
  void for_each(void (*cb)(Storage *s, void *ctx), void *ctx);
  void for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx);
  void for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx);
  void for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx);

  // Notification callbacks — fired whenever a device registers or unregisters.
  // Templatized so both std::function and pointer-sized forwarder structs are
  // accepted without forcing heap allocation.
  template<typename F> void add_on_registered_callback(F &&cb) { this->on_registered_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_unregistered_callback(F &&cb) { this->on_unregistered_.add(std::forward<F>(cb)); }

 protected:
  // Single allocation at set_device_count() — no realloc machinery
  FixedVector<Storage *> storages_;

  // LazyCallbackManager: 4-byte nullptr until first subscriber — saves RAM
  // on devices where no component listens for hotplug events
  LazyCallbackManager<void(Storage *)> on_registered_;
  LazyCallbackManager<void(Storage *)> on_unregistered_;
};

extern StorageRegistry *global_storage_registry;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::storage

#endif  // USE_ESP_IDF
