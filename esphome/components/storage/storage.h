#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <cstdio>
#include <memory>

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
  NOT_SUPPORTED,        // operation not supported by this driver/medium (e.g. format() on read-only)
  ALREADY_EXISTS,       // mkdir/create on a path that already exists
  NOT_EMPTY,            // non-recursive rmdir on a non-empty directory
  TOO_MANY_OPEN_FILES,  // no free FileHandle in the driver's handle pool
};

// fopen()-equivalent semantics — drivers must match these exactly:
//   READ       "r"   Open existing file for reading. Fails if it doesn't exist.
//   WRITE      "w"   Create file (or truncate existing) for writing.
//   APPEND     "a"   Create file if it doesn't exist; writes always go to the end.
//   READ_WRITE "r+"  Open existing file for reading and writing. Fails if it doesn't exist.
enum class OpenMode : uint8_t {
  READ = 0,
  WRITE,
  APPEND,
  READ_WRITE,
};

enum class SeekMode : uint8_t {
  SET = 0,  // absolute offset from start of file
  CUR,      // relative to current position
  END,      // relative to end of file
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

// Maximum filename length — covers NFSv3/v4 (255) and full FATFS LFN (255).
// All storage drivers must fit filenames within this bound.
static constexpr size_t STORAGE_NAME_MAX = 255;

struct FileStat {
  char name[STORAGE_NAME_MAX + 1];
  uint64_t size;
  uint32_t mtime{0};  // Unix timestamp (seconds); 0 if unavailable
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

// Abstract base — all storage drivers extend one of the three subclasses below.
//
// Contract: every virtual call on Storage and its subclasses is BLOCKING. Drivers do not
// run I/O on a separate task/thread. Consumers reading/writing large amounts of data (e.g.
// serving a large file over HTTP) must chunk their own calls (small len per read()/write()/
// read_chunk()/write_chunk()) and yield back to the main loop between chunks — a single call
// that blocks for too long will starve the watchdog and Wi-Fi/BLE stacks. This is the same
// reason copy()/read_file()/write_file() below exist as opt-in convenience helpers rather
// than being mandatory: they trade a single long blocking call for consumer simplicity, and
// are not appropriate to call from latency-sensitive contexts.
class Storage : public Component {
 public:
  // get_info() must succeed (return OK) even for a registered-but-unmounted device — e.g. a
  // FilesystemStorage before its medium is mounted, or a NetworkStorage before connect().
  // Report the unmounted state via StorageInfo::is_mounted = false, not via a non-OK error.
  // The registry logs/notifies based on registration, not on mount state (see
  // StorageRegistry::register_storage below) — an unmounted device must still be visible.
  virtual StorageError get_info(StorageInfo *info) = 0;
  virtual StorageType get_storage_type() const = 0;
};

// Offset-based byte access (raw flash, FRAM, EEPROM, NVS blobs)
class RawStorage : public Storage {
 public:
  StorageType get_storage_type() const override { return StorageType::RAW; }

  // Partial-read contract (applies to every read()/read_chunk() in this file): returning
  // StorageError::OK with *bytes_transferred < len means EOF was reached partway through —
  // this is not an error. A non-OK return means an actual I/O failure; *bytes_transferred is
  // unspecified in that case. Callers loop until *bytes_transferred == 0 or an error.
  virtual StorageError read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError erase(uint64_t offset, size_t len) = 0;
  virtual StorageError format() = 0;
};

// Common path-based operations shared by FilesystemStorage and NetworkStorage.
// Lets path-oriented consumers (e.g. a file browser/server) enumerate and operate
// on any storage that exposes a path namespace, regardless of local vs. network backing.
class PathStorage : public Storage {
 public:
  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  // callback returns false to stop enumeration early
  virtual StorageError list_dir(const char *path, bool (*callback)(const FileStat *entry, void *ctx), void *ctx) = 0;
  virtual StorageError mkdir(const char *path) = 0;
  // Non-recursive: must fail with StorageError::NOT_EMPTY if the directory has contents.
  // For recursive delete, use the free remove_recursive() helper below.
  virtual StorageError rmdir(const char *path) = 0;
  virtual StorageError remove(const char *path) = 0;
  virtual StorageError rename(const char *old_path, const char *new_path) = 0;
  // No copy() here — drivers only need to move bytes within their own device.
  // Cross-device copy (and same-device copy) is provided by the free copy() helper
  // below, built on read_file()/write_file().
};

// Path-based file access with a local filesystem layer (SD, USB, LittleFS partition).
// Also the right base for stateful network protocols that use handles/locks (e.g. SMB) —
// NetworkStorage below is for stateless protocols only.
class FilesystemStorage : public PathStorage {
 public:
  StorageType get_storage_type() const override { return StorageType::FILESYSTEM; }

  virtual StorageError mount() = 0;
  virtual StorageError unmount() = 0;
  virtual StorageError format() = 0;
  virtual StorageError sync() = 0;
  virtual StorageError open(const char *path, FileHandle *&handle, OpenMode mode) = 0;
  virtual StorageError close(FileHandle *handle) = 0;
  // Partial-read contract: see RawStorage::read() above.
  virtual StorageError read(FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError write(FileHandle *handle, const uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError seek(FileHandle *handle, int64_t offset, SeekMode mode = SeekMode::SET) = 0;
  virtual StorageError tell(FileHandle *handle, uint64_t *position) = 0;
};

// Path-based file access over a stateless network protocol (NFS) — no file handles, no
// connection-held locks. Stateful network protocols (e.g. SMB, which uses handles/locks)
// belong under FilesystemStorage instead, not here.
class NetworkStorage : public PathStorage {
 public:
  StorageType get_storage_type() const override { return StorageType::NETWORK; }

  virtual StorageError connect() = 0;
  virtual StorageError disconnect() = 0;
  // Partial-read contract: see RawStorage::read() above.
  virtual StorageError read_chunk(const char *path, uint8_t *buf, uint64_t offset, size_t len,
                                  size_t *bytes_transferred) = 0;
  virtual StorageError write_chunk(const char *path, const uint8_t *buf, uint64_t offset, size_t len,
                                   size_t *bytes_transferred) = 0;
};

// Runtime registry of all storage devices.
// Initializes before any driver (setup_priority::BUS) so drivers can safely
// call register_storage() / unregister_storage() from their own setup() or
// on hotplug events. Pool is sized exactly at codegen time from the number of
// configured devices — no compile-time upper bound, no wasted slots.
//
// Contract: the registry is NOT thread-safe. register_storage()/unregister_storage() and the
// for_each*() enumerations must only be called from the main loop task. This matters in
// particular for USB hotplug: the USB host stack's hotplug/disconnect callback may run on a
// different task than the main loop on some drivers — such drivers must defer their
// register_storage()/unregister_storage() call onto the main loop (e.g. via a flag polled in
// loop(), or App.scheduler) rather than calling it directly from the hotplug callback.
//
// Contract: "registered" means "usable". A driver must not call register_storage() until the
// device is ready to serve calls (get_info(), and for PathStorage subclasses, the path-based
// operations) — even if it's not yet mounted (see Storage::get_info() above, which must
// report unmounted state via StorageInfo::is_mounted rather than refusing to answer).
// Conversely, call unregister_storage() as soon as the device stops being usable (e.g. on USB
// disconnect), before it's torn down.
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
  // Both FILESYSTEM and NETWORK expose PathStorage — use this to browse/operate on
  // any path-based storage without caring whether it's local or network-backed.
  void for_each_path_based(void (*cb)(PathStorage *s, void *ctx), void *ctx);

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

//========================================================================
// Free helper functions — convenience wrappers over the Storage interfaces.
// None of these are virtual: drivers never implement or override them.
//========================================================================

// Human-readable name for a StorageError — use this in dump_config()/logging
// instead of hand-rolling a switch per driver.
const char *error_to_string(StorageError error);

// stat()-based existence/size checks — thin wrappers, work on any PathStorage
// (FilesystemStorage or NetworkStorage).
bool exists(PathStorage *storage, const char *path);
StorageError file_size(PathStorage *storage, const char *path, uint64_t *size);

// Reads an entire file in one call. Allocates a buffer sized from stat() internally
// (heap allocation — do not call from hot paths or after setup() on the main loop;
// intended for occasional whole-file reads, e.g. serving a file over HTTP).
// On success, *out owns the buffer and *size holds the number of bytes read.
StorageError read_file(FilesystemStorage *storage, const char *path, std::unique_ptr<uint8_t[]> &out, size_t *size);
StorageError read_file(NetworkStorage *storage, const char *path, std::unique_ptr<uint8_t[]> &out, size_t *size);

// Writes an entire buffer to a file in one call (create/truncate semantics, like
// OpenMode::WRITE).
StorageError write_file(FilesystemStorage *storage, const char *path, const uint8_t *data, size_t size);
StorageError write_file(NetworkStorage *storage, const char *path, const uint8_t *data, size_t size);

// Copies a file, within the same storage or across two different storages (e.g. SD -> USB,
// USB -> NFS). Dispatches on get_storage_type() and goes through read_file()/write_file(),
// so it shares their heap-allocation and "don't call from hot paths" caveats.
StorageError copy(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path);

// Recursively removes a directory and everything under it, via list_dir() + remove() /
// remove_recursive() (for subdirectories) + a final non-recursive rmdir(). Drivers only
// need to implement the non-recursive rmdir() primitive; use this when you need recursive
// delete semantics.
StorageError remove_recursive(PathStorage *storage, const char *path);

}  // namespace esphome::storage
