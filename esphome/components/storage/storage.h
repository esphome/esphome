#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace esphome::storage {

// Values mirror POSIX errno codes (only the ones actually used here are defined) so a
// driver's error can be reasoned about against familiar numbers — this is not an errno
// passthrough (drivers still return StorageError, not int), it's just keeping the integer
// values consistent with their closest POSIX equivalent. Values without a natural errno
// counterpart (OK, WRITE_ERROR, TRANSFER_TOO_LARGE) get their own, non-colliding numbers
// above the highest errno in use here, so no accidental collisions creep in later either.
enum class StorageError : uint8_t {
  OK = 0,
  NOT_FOUND = ENOENT,            // 2
  READ_ERROR = EIO,              // 5 — I/O error
  PERMISSION_DENIED = EACCES,    // 13
  ALREADY_EXISTS = EEXIST,       // 17
  NOT_READY = ENODEV,            // 19
  INVALID_ARGS = EINVAL,         // 22
  TOO_MANY_OPEN_FILES = EMFILE,  // 24
  NO_SPACE = ENOSPC,             // 28
  NOT_EMPTY = ENOTEMPTY,         // 39
  CORRUPT = EILSEQ,              // 84 on newlib/ESP-IDF — closest POSIX equivalent (illegal byte sequence)
  NOT_SUPPORTED = ENOTSUP,       // 95 on newlib/ESP-IDF
  TIMEOUT = ETIMEDOUT,           // 116 on newlib/ESP-IDF
  // No distinct POSIX errno for a write-direction I/O error (EIO is used by READ_ERROR
  // above) — assigned its own value past the highest errno referenced here.
  WRITE_ERROR = 120,
  TRANSFER_TOO_LARGE = 121,  // no POSIX equivalent: transfer rejected by max_blocking_transfer_size
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

// Chunk size for the streaming copy() helper; multiple of 512 so FATFS can do direct
// whole-sector transfers. Overridable from YAML (storage: copy_chunk_size), which sets
// the USE_STORAGE_COPY_CHUNK_SIZE define via codegen; falls back to 16 kB otherwise
// (also covers clang-tidy, which never sees generated defines).
#ifdef USE_STORAGE_COPY_CHUNK_SIZE
static constexpr size_t STORAGE_COPY_CHUNK_SIZE = USE_STORAGE_COPY_CHUNK_SIZE;
#else
static constexpr size_t STORAGE_COPY_CHUNK_SIZE = 16384;
#endif

// Max directory nesting depth for remove_recursive() below. Stack cost per level is
// driver-dependent — it's not just the path buffer and call frame recorded here, but also
// whatever the driver's own list_dir()/remove()/rmdir() stack up per call (e.g. sd_storage's
// LFN buffers put this closer to ~1.3kB/level than the ~600B this constant alone would
// suggest). ESP32 task stacks are typically only 8-16kB, so this default is chosen
// conservatively across drivers rather than tuned to the cheapest one.
static constexpr size_t STORAGE_MAX_RECURSION_DEPTH = 8;

struct FileStat {
  // Basename of the entry only (e.g. "file.txt"), never a full/relative path — this holds for
  // both stat()'s result and every entry list_dir() passes to its callback. Callers that need
  // the full path must join it themselves (e.g. with the directory path passed to list_dir()).
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

// Optional driver capabilities, reported via Storage::get_capabilities().
// Bitmask so future capabilities can be added without new virtuals.
enum StorageCaps : uint8_t {
  // The driver's data-plane methods (see the control-/data-plane contract below) may be
  // called from a task other than the main loop, provided all calls to this instance are
  // externally serialized. Only set this if the driver's I/O path has no hidden
  // main-loop affinity — e.g. NOT safe if the driver shares a bus (SPI/I2C) with
  // components that access it from the main loop.
  STORAGE_CAP_IO_TASK_SAFE = 1 << 0,
};

// Abstract base — all storage drivers extend one of the three subclasses below.
//
// Contract: every call is BLOCKING — drivers do not run I/O on a separate task/thread.
// Consumers moving large amounts of data must chunk their own calls and yield between
// chunks (large len values will starve the watchdog and Wi-Fi/BLE); copy()/read_file()/
// write_file() below are opt-in convenience helpers for this, not mandatory, and are not
// appropriate from latency-sensitive contexts.
//
// Control plane vs. data plane: get_info(), stat(), list_dir(), mkdir(), rmdir(), remove(),
// rename(), open(), close(), read(), write(), seek(), tell(), sync(), read_chunk(), and
// write_chunk() are DATA-PLANE — task-agnostic by design. They must not touch main-loop-only
// facilities (scheduler, the storage registry, other components), must not assume a
// particular calling task, and must do no hidden control-plane work (e.g. no lazy mount() on
// first read() — a data-plane call on an unmounted/disconnected device returns
// StorageError::NOT_READY instead). This is what lets them run on a future dedicated worker
// task instead of the main loop. Calls on one Storage instance are externally serialized by
// the caller (main loop today, later the worker) — drivers don't need to be thread-safe, just
// tolerant of running on a single, possibly-different task. setup(), mount()/unmount(),
// format(), connect()/disconnect(), and all StorageRegistry calls are CONTROL-PLANE and
// remain main-loop-only (see StorageRegistry's contract below). The async worker (storage_worker.h
// in this component, compiled in as USE_STORAGE_WORKER when a path-based driver requests it — see
// request_storage_worker() in __init__.py) only offloads data-plane I/O to its own task for
// storages reporting STORAGE_CAP_IO_TASK_SAFE; all others continue to be driven from the main loop.
class Storage : public Component {
 public:
  // Must succeed (return OK) even when registered-but-unmounted/-disconnected — report that
  // via StorageInfo::is_mounted = false, not a non-OK error (see StorageRegistry below).
  virtual StorageError get_info(StorageInfo *info) = 0;
  virtual StorageType get_storage_type() const = 0;

  // Bitwise OR of StorageCaps. Default 0: drivers must explicitly opt in to
  // capabilities — a driver that never considered task-safety is never treated as such.
  virtual uint8_t get_capabilities() const { return 0; }
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
// Optional interface for drivers whose medium can be mounted/unmounted at runtime (removable
// media: SD cards, USB sticks, network shares). Drivers inherit this IN ADDITION to their
// storage base class. Consumers reach it via PathStorage::as_mountable() below — the no-RTTI
// downcast hook (ESPHome builds with -fno-rtti, so dynamic_cast is unavailable).
// NOTE: FilesystemStorage declares mount()/unmount() with identical signatures as part of its
// own contract; a driver inheriting both provides ONE override that satisfies both bases (the
// standard sibling-interface pattern). This interface exists as an explicit opt-in marker:
// non-removable filesystems simply don't inherit it.
class MountableStorage {
 public:
  // Which of the two operations may be invoked externally (YAML actions, web UI). A driver
  // whose medium controls its own mount lifecycle (e.g. USB: hotplug auto-mounts on insertion)
  // supports only UNMOUNT ("safe eject"); manually managed media (SD card, NFS export) support
  // both. Consumers building UI must gate each button on its bit — as_mountable() != nullptr
  // alone only says "at least one of the two works".
  static constexpr uint8_t MOUNT_CAP_MOUNT = 1 << 0;
  static constexpr uint8_t MOUNT_CAP_UNMOUNT = 1 << 1;

  virtual ~MountableStorage() = default;
  virtual uint8_t get_mount_caps() const { return MOUNT_CAP_MOUNT | MOUNT_CAP_UNMOUNT; }
  virtual StorageError mount() = 0;
  virtual StorageError unmount() = 0;
};

class PathStorage : public Storage {
 public:
  // No-RTTI downcast hook: consumers holding a PathStorage* (e.g. from resolve_path() or
  // for_each_path_based()) use this to reach the optional MountableStorage interface —
  // dynamic_cast is unavailable (ESPHome builds with -fno-rtti). Drivers that inherit
  // MountableStorage override this with `return this;`.
  virtual MountableStorage *as_mountable() { return nullptr; }

  // The VFS mount point this storage is reachable under (e.g. "/sdcard", "/usb"). Set once by
  // the driver (typically from YAML) and treated as invariant for the lifetime of the instance —
  // it does not change across mount()/unmount() cycles. Contract, enforced by the driver at set
  // time: must start with '/', must not end with '/', and must not be "" or "/".
  const char *get_mount_path() const { return this->mount_path_; }

  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  // Contract: drivers must NOT emit "." or ".." entries — tree-walkers such as
  // remove_recursive() below rely on this to avoid recursing forever. Entry order is
  // unspecified (driver/filesystem dependent) — do not rely on any particular ordering.
  // The callback returns false to stop enumeration early; that is not an error condition —
  // list_dir() itself still returns StorageError::OK in that case.
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

 protected:
  // Stores the mount path — called once by the driver (codegen-driven setter), never at
  // runtime. The caller is responsible for satisfying the contract documented on
  // get_mount_path() above (starts with '/', does not end with '/', not "" or "/"); this is a
  // configuration-time invariant validated by the driver's Python codegen, not re-checked here.
  void set_mount_path_(const char *path) { this->mount_path_ = path; }

  const char *mount_path_{nullptr};
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
  virtual StorageError seek(FileHandle *handle, int64_t offset, SeekMode mode) = 0;
  StorageError seek(FileHandle *handle, int64_t offset) { return seek(handle, offset, SeekMode::SET); }
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

// Runtime registry of all storage devices. Initializes before any driver
// (setup_priority::BUS); pool is sized exactly at codegen time from the number of configured
// devices — no compile-time upper bound, no wasted slots.
//
// Contract: main-loop-only — register_storage()/unregister_storage()/for_each*() must never
// be called from another task (e.g. a USB hotplug callback must defer onto the main loop
// instead of calling directly). Contract: "registered" means "usable" — call
// register_storage() once the device can serve calls (even if unmounted, per
// Storage::get_info() above), and unregister_storage() as soon as it stops being usable,
// before teardown.
//
// Contract: unregister_storage() does not return until every subscriber has had a chance to
// stop using the storage being removed — in particular, the async worker (storage_worker.h, if
// compiled in — see USE_STORAGE_WORKER) drains any in-flight or queued data-plane call against
// it (best effort: it gives up after a bounded timeout if a call refuses to finish, logging an
// error, since at that point the medium is presumed gone anyway). This is what makes
// "unregister, then unmount" provably safe for the calling driver: by the time this function
// returns, no async consumer still holds an open handle or is mid-call against the storage.
class StorageRegistry : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }

  // Called by codegen with the exact number of configured storage devices
  void set_device_count(size_t count) { this->storages_.init(count); }

  // Guard-rail for the BLOCKING helpers below (read_file()/write_file()/copy()/move()):
  // transfers larger than this are rejected with StorageError::TRANSFER_TOO_LARGE instead of
  // freezing the node, so callers get routed through the async worker (storage_worker.h)
  // instead. 0 (default) means unlimited — preserves current behavior.
  void set_max_blocking_transfer_size(uint64_t size) { this->max_blocking_transfer_size_ = size; }
  uint64_t get_max_blocking_transfer_size() const { return this->max_blocking_transfer_size_; }

  // Returns OK on success (idempotent: re-registering an already-registered device is OK),
  // INVALID_ARGS for nullptr, NO_SPACE when the registry is at its codegen-sized capacity —
  // the latter indicates a codegen/runtime device-count mismatch; drivers should treat it as
  // fatal (log + mark_failed()) instead of running with an invisibly missing device.
  StorageError register_storage(Storage *s);
  void unregister_storage(Storage *s);
  // Drain-only variant of unregister_storage() for drivers whose registration is permanent
  // but whose medium comes and goes (SD safe-eject, NFS unmount): fires the same drain
  // callbacks — by return, no in-flight data-plane call against `s` remains and pending
  // worker requests touching it are completed with NOT_READY — but the entry stays
  // registered. The driver flips its mounted state right after; subsequent data-plane calls
  // fail with NOT_READY at the driver, so staying registered is safe. No-op if `s` is not
  // registered. Avoids the unregistered/registered churn (log noise, consumers briefly
  // seeing the device vanish) of the unregister+re-register pattern.
  void quiesce_storage(Storage *s);
  bool is_registered(const Storage *s) const;

  // Stable enumeration by index — pairs with get_mount_path() on PathStorage entries to let a
  // caller build its own index <-> mountpoint view without needing a for_each() callback.
  size_t size() const { return this->storages_.size(); }
  Storage *get(size_t index) const { return this->storages_[index]; }

  // Enumerate by type — callback receives each matching device and caller ctx
  void for_each(void (*cb)(Storage *s, void *ctx), void *ctx);
  void for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx);
  void for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx);
  void for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx);
  // Both FILESYSTEM and NETWORK expose PathStorage — use this to browse/operate on
  // any path-based storage without caring whether it's local or network-backed.
  void for_each_path_based(void (*cb)(PathStorage *s, void *ctx), void *ctx);
  // Typed overload: additionally hands the StorageType to the callback, sparing consumers the
  // per-entry virtual get_storage_type() call when they need to distinguish local vs. network.
  void for_each_path_based(void (*cb)(PathStorage *s, StorageType type, void *ctx), void *ctx);

  // Longest-prefix match of vfs_path against every registered PathStorage's mount point.
  // Prefix matches only at a '/' boundary or an exact match (e.g. "/sd2/x" does NOT match a
  // mount point of "/sd"), so distinct mount points sharing a common prefix can't shadow each
  // other. Returns nullptr (with *rel_out left untouched) if no registered mount point matches.
  // On a match, *rel_out points into vfs_path: "" when vfs_path IS the mount point exactly,
  // otherwise the remainder starting with '/'.
  PathStorage *resolve_path(const char *vfs_path, const char **rel_out);

  // Builds the canonical VFS path for a PathStorage's mount point plus a relative path: the
  // mount path with `rel` appended, normalizing the join so there's exactly one '/' between
  // them regardless of whether `rel` itself starts with '/'. Writes into `out` (size `len`) and
  // returns false (leaving `out` unspecified) if the result wouldn't fit.
  static bool build_path(const PathStorage *s, const char *rel, char *out, size_t len);

  // Notification callbacks — fired whenever a device registers or unregisters.
  // Templatized so both std::function and pointer-sized forwarder structs are
  // accepted without forcing heap allocation.
  template<typename F> void add_on_registered_callback(F &&cb) { this->on_registered_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_unregistered_callback(F &&cb) { this->on_unregistered_.add(std::forward<F>(cb)); }

 protected:
  // Single allocation at set_device_count() — no realloc machinery
  FixedVector<Storage *> storages_;
  // Guards storages_ against the one cross-thread access pattern: the worker task reads via
  // is_registered() (per-chunk cancellation check) while the main loop mutates via
  // register_storage()/unregister_storage(). All other accessors (for_each*, size()/get(),
  // resolve_path()) are main-loop-only by contract and need no lock — the task never calls them.
  mutable Mutex registry_lock_;

  // LazyCallbackManager: 4-byte nullptr until first subscriber — saves RAM
  // on devices where no component listens for hotplug events
  LazyCallbackManager<void(Storage *)> on_registered_;
  LazyCallbackManager<void(Storage *)> on_unregistered_;

  uint64_t max_blocking_transfer_size_{0};  // 0 = unlimited
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
// exists(): only StorageError::NOT_FOUND maps to a clean "no". Any OTHER non-OK error
// (NOT_READY, READ_ERROR, PERMISSION_DENIED, ...) also returns false but is surfaced via
// err_out so callers can distinguish "file is absent" from "medium is unmounted/faulted"
// before deciding to create/overwrite or to report 'not found'.
bool exists(PathStorage *storage, const char *path, StorageError *err_out = nullptr);
StorageError file_size(PathStorage *storage, const char *path, uint64_t *size);

// Deleter that frees memory obtained from RAMAllocator<uint8_t> (malloc/heap_caps_malloc_prefer)
// rather than operator delete[] — required because RamBuffer below is backed by RAMAllocator,
// not `new[]`.
struct RamBufferDeleter {
  size_t size{0};  // a default-constructed RamBuffer must carry a well-defined (unused) size
  void operator()(uint8_t *ptr) const {
    if (ptr != nullptr)
      RAMAllocator<uint8_t>().deallocate(ptr, size);
  }
};
using RamBuffer = std::unique_ptr<uint8_t[], RamBufferDeleter>;

// Reads an entire file in one call. Allocates the buffer via RAMAllocator internally (nothrow —
// returns StorageError::NO_SPACE on allocation failure rather than throwing/aborting, since
// ESPHome builds with exceptions disabled). Do not call from hot paths or after setup() on the
// main loop; intended for occasional whole-file reads, e.g. serving a file over HTTP.
// On success, *out owns the buffer and *size holds the number of bytes read.
StorageError read_file(FilesystemStorage *storage, const char *path, RamBuffer &out, size_t *size);
StorageError read_file(NetworkStorage *storage, const char *path, RamBuffer &out, size_t *size);
// PathStorage overload: dispatches on get_storage_type(), same as copy()/move() below. Lets a
// generic path-based consumer (e.g. a file browser holding a PathStorage*) call read_file()
// without having to downcast to the concrete subtype itself first.
StorageError read_file(PathStorage *storage, const char *path, RamBuffer &out, size_t *size);

// Writes an entire buffer to a file in one call (create/truncate semantics, like
// OpenMode::WRITE).
StorageError write_file(FilesystemStorage *storage, const char *path, const uint8_t *data, size_t size);
StorageError write_file(NetworkStorage *storage, const char *path, const uint8_t *data, size_t size);
// PathStorage overload — see read_file(PathStorage *, ...) above.
StorageError write_file(PathStorage *storage, const char *path, const uint8_t *data, size_t size);

// Copies a file, within the same storage or across two different storages (e.g. SD -> USB,
// USB -> NFS). Dispatches on get_storage_type() and streams the file through a fixed
// STORAGE_COPY_CHUNK_SIZE buffer (unlike read_file()/write_file(), it never holds the whole
// file in RAM), feeding the task watchdog between chunks. It still BLOCKS the main loop for
// the duration of the copy, though — large copies freeze sensor updates/API responsiveness
// unless the caller uses the async worker's copy (StorageWorker::async_copy(), storage_worker.h)
// instead.
StorageError copy(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path);

// Moves a file, within the same storage or across two different storages. Same-storage moves
// go through rename() directly — no chunk buffer, no size limit, near-O(1). Cross-storage moves
// go through copy() (inheriting its chunking, watchdog feeding, and max_blocking_transfer_size
// enforcement) followed by remove() on the source, but ONLY if the copy succeeded. If the
// source remove() fails, its error is returned and the destination copy is kept — the caller
// ends up with the file in both places rather than in neither.
StorageError move(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path);

// Recursively removes a directory and everything under it, via list_dir() + remove() /
// remove_recursive() (for subdirectories) + a final non-recursive rmdir(). Drivers only
// need to implement the non-recursive rmdir() primitive; use this when you need recursive
// delete semantics. Aborts with StorageError::INVALID_ARGS if the tree is nested deeper than
// STORAGE_MAX_RECURSION_DEPTH (see its comment above for why).
StorageError remove_recursive(PathStorage *storage, const char *path);

}  // namespace esphome::storage
