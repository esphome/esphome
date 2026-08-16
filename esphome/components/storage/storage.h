#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace esphome::storage {

// The closest POSIX errno is noted per entry; error_from_errno() below does the actual
// translation. Values are sequential rather than errno numbers: errno numbering is
// toolchain-specific, and nothing here persists or transmits them (error_to_string() is what
// leaves the device).
enum class StorageError : uint8_t {
  OK = 0,
  NOT_FOUND,            // ENOENT
  READ_ERROR,           // EIO
  PERMISSION_DENIED,    // EACCES
  ALREADY_EXISTS,       // EEXIST
  NOT_READY,            // ENODEV
  INVALID_ARGS,         // EINVAL
  TOO_MANY_OPEN_FILES,  // EMFILE
  NO_SPACE,             // ENOSPC
  NOT_EMPTY,            // ENOTEMPTY
  CORRUPT,              // EILSEQ (illegal byte sequence)
  NOT_SUPPORTED,        // ENOTSUP
  TIMEOUT,              // ETIMEDOUT
  // No distinct POSIX errno for a write-direction I/O error -- EIO is used by READ_ERROR above.
  WRITE_ERROR,
  TRANSFER_TOO_LARGE,  // no POSIX equivalent: transfer rejected by max_blocking_transfer_size
  VERIFY_MISMATCH,     // no POSIX equivalent: post-write read-back did not match the source
};

// fopen()-equivalent semantics -- drivers must match these exactly:
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

// Identifies the concrete subtype of a Storage pointer without Run Time Type Information.
// Used by StorageRegistry::for_each_filesystem/raw/network() to safely
// static_cast without dynamic_cast (Run Time Type Information is disabled on ESP32).
enum class StorageType : uint8_t {
  RAW = 0,
  FILESYSTEM,
  NETWORK,
  KEY_VALUE,
};

struct StorageInfo {
  const char *id;
  const char *name;
  // Stable machine-readable medium kind ("sd", "usb", "nfs", "flash", ...) for consumers that
  // must distinguish media (e.g. to pick an icon). Not a display string, never user-configured.
  // Optional: nullptr (all callers zero-init StorageInfo) means fall back to get_storage_type().
  const char *kind;
  uint64_t total_bytes;
  uint64_t free_bytes;
  uint32_t block_size;
  bool is_mounted;
  bool is_removable;
  bool is_read_only;
};

// Maximum filename length -- covers NFSv3/v4 (255) and full FATFS LFN (255).
// All storage drivers must fit filenames within this bound.
static constexpr size_t STORAGE_NAME_MAX = 255;

// Base streaming/copy chunk; multiple of 512 for FATFS direct whole-sector transfers. YAML
// override (storage: copy_chunk_size); codegen default 16 kB (the most a slow SD write clears in
// one 20 ms loop slice). Platform sizing lives in alloc_dma_capable(), not here. The value below
// is only the fallback for builds that never see the generated define (clang-tidy).
#ifdef USE_STORAGE_COPY_CHUNK_SIZE
static constexpr size_t STORAGE_COPY_CHUNK_SIZE = USE_STORAGE_COPY_CHUNK_SIZE;
#else
static constexpr size_t STORAGE_COPY_CHUNK_SIZE = 16384;
#endif

// Configured device count -- sizes the registry and the for_each* stack snapshots. Set by
// codegen; fallback for builds without the generated define (clang-tidy, IDEs). Never 0 (a
// zero-length array is not valid C++).
#if defined(USE_STORAGE_MAX_DEVICES) && USE_STORAGE_MAX_DEVICES > 0
static constexpr size_t STORAGE_MAX_DEVICES = USE_STORAGE_MAX_DEVICES;
#else
static constexpr size_t STORAGE_MAX_DEVICES = 8;
#endif

// Longest path the interface buffers carry (tree walks, worker slots): MAX over configured
// drivers, not min -- a driver with a tighter limit refuses its own over-long paths. Over-long
// is INVALID_ARGS, never truncated. 256 = ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN; fallback until
// codegen derives it.
#if defined(USE_STORAGE_PATH_MAX) && USE_STORAGE_PATH_MAX > 0
static constexpr size_t STORAGE_PATH_MAX = USE_STORAGE_PATH_MAX;
#else
static constexpr size_t STORAGE_PATH_MAX = 256;
#endif

// Longest FULL VFS path (mount point + relative), the form build_path() writes. STORAGE_PATH_MAX
// bounds the driver-RELATIVE path only; a full path is longer by the mount point, sized at codegen
// (register_mount_path() in __init__.py). Reusing STORAGE_PATH_MAX here would make build_path()
// refuse silently as the relative path nears its bound.
#if defined(USE_STORAGE_VFS_PATH_MAX) && USE_STORAGE_VFS_PATH_MAX > 0
static constexpr size_t STORAGE_VFS_PATH_MAX = USE_STORAGE_VFS_PATH_MAX;
#else
static constexpr size_t STORAGE_VFS_PATH_MAX = STORAGE_PATH_MAX + 32;
#endif

// Max directory nesting for the tree walks (copy(), remove_recursive()), budgeted against the
// stack they run on. Path buffers cost a flat 2 * STORAGE_PATH_MAX (allocated once, extended per
// level -- append_path_segment in storage.cpp); what scales with depth is the walk frame plus the
// driver's list_dir()/remove() frames, ~830 B/level (sd_storage's FATFS LFN buffers dominate).
// Five levels ~4.7 kB against the 8 kB worker/loop task stacks. Deeper trees are refused with
// INVALID_ARGS rather than risking a stack overflow.
#if defined(USE_STORAGE_MAX_RECURSION_DEPTH) && USE_STORAGE_MAX_RECURSION_DEPTH > 0
static constexpr size_t STORAGE_MAX_RECURSION_DEPTH = USE_STORAGE_MAX_RECURSION_DEPTH;
#else
static constexpr size_t STORAGE_MAX_RECURSION_DEPTH = 4;
#endif

struct FileStat {
  // Basename of the entry only (e.g. "file.txt"), never a full/relative path -- this holds for
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
  FILE *file{nullptr};  // POSIX handle -- valid for all VFS-backed drivers, nullptr otherwise
  // Drivers bypassing VFS subclass this and add their own handle (lfs_file_t, etc.)
};

// Optional driver capabilities, reported via Storage::get_capabilities().
// Bitmask so future capabilities can be added without new virtuals.
enum StorageCaps : uint8_t {
  // The driver's data-plane methods may run on a task other than the main loop, provided all
  // calls to this instance are externally serialized. Set only if the I/O path has no hidden
  // main-loop affinity -- NOT safe if the driver shares a bus (SPI/I2C) with main-loop components.
  STORAGE_CAP_IO_TASK_SAFE = 1 << 0,
};

// Abstract base -- every driver extends one of the three subclasses below.
//
// Every call is BLOCKING (no driver-internal task/thread). Consumers moving large data must chunk
// and yield between chunks (large len starves the watchdog and Wi-Fi/BLE); copy()/read_file()/
// write_file() are opt-in helpers for this, not for latency-sensitive contexts.
//
// DATA-PLANE (get_info, stat, list_dir, mkdir, rmdir, remove, rename, open, close, read, write,
// seek, tell, sync, read_chunk, write_chunk): task-agnostic. Must not touch main-loop-only
// facilities (scheduler, registry, other components), assume a particular calling task, or do
// hidden control-plane work (no lazy mount() on first read() -- an unmounted device returns
// NOT_READY). This is what lets them run on the worker task. Calls on one instance are externally
// serialized by the caller, so drivers need not be thread-safe, only tolerant of a possibly-
// different task. CONTROL-PLANE (setup, mount/unmount, format, connect/disconnect, all
// StorageRegistry calls) stays main-loop-only (see StorageRegistry below). The worker
// (storage_worker.h, USE_STORAGE_WORKER when a path driver requests it -- request_storage_worker()
// in __init__.py) offloads data-plane I/O to its task only for storages reporting
// STORAGE_CAP_IO_TASK_SAFE; all others stay on the main loop.
class Storage : public Component {
 public:
  // Must succeed (return OK) even when registered-but-unmounted/-disconnected -- report that
  // via StorageInfo::is_mounted = false, not a non-OK error (see StorageRegistry below).
  virtual StorageError get_info(StorageInfo *info) = 0;
  virtual StorageType get_storage_type() const = 0;

  // Bitwise OR of StorageCaps. Default 0: drivers must explicitly opt in to
  // capabilities -- a driver that never considered task-safety is never treated as such.
  virtual uint8_t get_capabilities() const { return 0; }
};

// What a raw medium's erase() accepts -- NOT how it does it: which opcode (chip/block/sector)
// runs is the driver's business. Consumers use these bits only to decide what to offer, e.g.
// whether to show an erase control at all, and whether a write has to be preceded by one.
enum RawEraseCaps : uint8_t {
  // Writes only turn bits one way and must be preceded by erasing the covering unit (NOR
  // flash). Media without this bit overwrite in place (FRAM, EEPROM) and have no erase at all.
  RAW_WRITE_NEEDS_ERASE = 1 << 0,
  RAW_ERASE_SECTOR = 1 << 1,  // smallest erasable unit, see RawGeometry::erase_sector
  RAW_ERASE_BLOCK = 1 << 2,   // larger erase unit, see RawGeometry::erase_block
  RAW_ERASE_CHIP = 1 << 3,    // whole device in one command
};

// Geometry of a raw medium. Raw media differ fundamentally -- a FRAM is byte-addressable with
// no erase whatsoever, a NOR flash erases in sectors and needs it before every write -- so
// consumers (actions, HTTP API, browser) must ask instead of assuming flash semantics.
struct RawGeometry {
  uint64_t capacity{0};
  uint32_t write_page{1};    // write granularity/alignment in bytes; 1 = byte addressable
  uint32_t erase_sector{0};  // smallest erasable unit; 0 = medium has no erase
  uint32_t erase_block{0};   // larger erase unit; 0 = none
  uint8_t caps{0};           // RawEraseCaps bitmask
};

// Offset-based byte access (raw flash, FRAM, EEPROM, NVS blobs).
//
// Permanently attached (soldered I2C/SPI/OneWire, or a fixed flash region): a RawStorage registers
// once for the node's lifetime, does not inherit MountableStorage, and is never unregistered or
// quiesced -- so consumers needn't watch for it to disappear and the worker's drain never matches a
// raw device. Contention still applies: two operations on one chip must not overlap.
class RawStorage : public Storage {
 public:
  StorageType get_storage_type() const override { return StorageType::RAW; }

  // Partial-read contract (applies to every read()/read_chunk() in this file): returning
  // StorageError::OK with *bytes_transferred < len means EOF was reached partway through --
  // this is not an error. A non-OK return means an actual I/O failure; *bytes_transferred is
  // unspecified in that case. Callers loop until *bytes_transferred == 0 or an error.
  virtual StorageError read(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;
  virtual StorageError write(uint64_t offset, const uint8_t *buf, size_t len, size_t *bytes_transferred) = 0;

  // Erases [offset, offset+len).
  //
  // Contract:
  //  - Media with no RAW_ERASE_* capability return NOT_SUPPORTED: they overwrite in place, so
  //    there is nothing to erase, and OK would falsely claim the range is blank.
  //  - The range must be erase_sector-aligned at both ends -- erasing is destructive at that
  //    granularity, so an unaligned request would take neighbouring data; returns INVALID_ARGS.
  //  - Within a valid range the driver picks the coarsest opcode that fits.
  virtual StorageError erase(uint64_t offset, size_t len) = 0;
  virtual StorageError format() = 0;

  // Every driver answers for its own medium -- consumers must not infer geometry from the type.
  virtual void get_raw_geometry(RawGeometry *out) const = 0;

#ifdef USE_STORAGE_DEVICE_NODES
  // Whether this medium appears as its own node in a file browser. Presentation, not storage: a
  // raw device has no path namespace, so the node just hangs its address-based operations. Off by
  // default; only meaningful when a browser is configured (USE_STORAGE_DEVICE_NODES).
  virtual bool has_device_node() const { return false; }
  // Label of that node -- not the YAML id, not the entity name; it names only this device's
  // browser node. nullptr when the driver has no node.
  virtual const char *get_device_node_name() const { return nullptr; }
#endif
};

// Opaque blobs addressed by a 32-bit key, NOT by offset or path. A sibling of RawStorage, not a
// subclass -- byte-addressed read()/write() do not apply, and modelling it as RawStorage would be an
// is-a lie. Boot-guaranteed like RawStorage: no MountableStorage, registers once for the node's
// lifetime, so consumers needn't watch for removal. NVS is the natural backend; raw/littlefs may
// emulate it later. Contention applies (two ops on one backend must not overlap).
//
// The key is a caller-supplied uint32 (the preferences key space); deriving a collision-free key
// from a semantic id is the consumer's job.
class KeyValueStorage : public Storage {
 public:
  StorageType get_storage_type() const override { return StorageType::KEY_VALUE; }

  // Read the value for `key` into `buf` (capacity `len`); on success *got holds the byte count.
  // NOT_FOUND if the key is absent; INVALID_ARGS if the stored value is larger than `len` (query
  // get_size() first for values of unknown length).
  virtual StorageError get(uint32_t key, uint8_t *buf, size_t len, size_t *got) = 0;

  // Store `len` bytes under `key`, replacing any existing value.
  virtual StorageError set(uint32_t key, const uint8_t *data, size_t len) = 0;

  // Remove `key`. Removing an absent key is a no-op success (idempotent).
  virtual StorageError erase(uint32_t key) = 0;

  // Whether `key` currently has a value, without reading it.
  virtual bool has(uint32_t key) = 0;

  // Byte length of the value for `key` into *out. NOT_FOUND if the key is absent.
  virtual StorageError get_size(uint32_t key, size_t *out) = 0;

  // Enumerate every stored key, invoking `callback` once per key with its value's byte length.
  // Return false from the callback to stop the walk early (mirrors list_dir()); list_keys() itself
  // still returns StorageError::OK in that case. The value is not read here -- a consumer that wants
  // it calls get() with the reported size. Iteration order is backend-defined and not stable across
  // writes; a key rewritten in place is reported once, with its current length.
  virtual StorageError list_keys(bool (*callback)(uint32_t key, size_t size, void *ctx), void *ctx) = 0;

  // Runtime bring-up: detect an empty/invalid medium and initialize it in place -- a fast no-op for
  // a flash-time-laid-out partition, real work for an external bus device on first boot once its bus
  // is up. Called at backend setup, never from codegen.
  virtual StorageError ensure_initialized() = 0;

  // Destructive: wipe and recreate an empty store.
  virtual StorageError format() = 0;
};

// Path-based operations shared by FilesystemStorage and NetworkStorage, so path-oriented consumers
// (e.g. a file browser) enumerate and operate on any path namespace, local or network.
//
// Optional mount/unmount interface for removable media (SD, USB, network shares). Inherited IN
// ADDITION to the storage base; reached via PathStorage::as_mountable() below -- the no-RTTI
// downcast hook (ESPHome builds with -fno-rtti). FilesystemStorage declares mount()/unmount() too,
// so a driver inheriting both provides ONE override satisfying both bases. An explicit opt-in
// marker: non-removable filesystems don't inherit it.
class MountableStorage {
 public:
  // Which of the two operations may be invoked externally (YAML actions, web UI). USB (hotplug
  // auto-mounts) supports only UNMOUNT ("safe eject"); manually managed media (SD, NFS) support
  // both. Consumers gate each UI button on its bit -- as_mountable() != nullptr only says "at
  // least one works".
  static constexpr uint8_t MOUNT_CAP_MOUNT = 1 << 0;
  static constexpr uint8_t MOUNT_CAP_UNMOUNT = 1 << 1;

  virtual ~MountableStorage() = default;
  virtual uint8_t get_mount_caps() const { return MOUNT_CAP_MOUNT | MOUNT_CAP_UNMOUNT; }
  virtual StorageError mount() = 0;
  virtual StorageError unmount() = 0;
};

class FilesystemStorage;  // forward -- PathStorage::as_filesystem() below

class PathStorage : public Storage {
 public:
  // No-RTTI downcast hook to the optional MountableStorage interface (dynamic_cast is unavailable,
  // -fno-rtti). Drivers inheriting MountableStorage override with `return this;`.
  virtual MountableStorage *as_mountable() { return nullptr; }
  // No-RTTI downcast to the filesystem interface (format/sync/open...). nullptr for
  // path-based storages that are not filesystems.
  virtual FilesystemStorage *as_filesystem() { return nullptr; }

  // The VFS mount point this storage is reachable under (e.g. "/sdcard"). Set once by the driver
  // (typically from YAML), invariant for the instance lifetime (unchanged across mount/unmount).
  // Contract, enforced at set time: starts with '/', does not end with '/', not "" or "/".
  const char *get_mount_path() const { return this->mount_path_; }

  virtual StorageError stat(const char *path, FileStat *stat) = 0;
  // Contract: drivers must NOT emit "." or ".." (tree-walkers like remove_recursive() rely on this
  // to avoid infinite recursion). Entry order is unspecified. The callback returning false stops
  // enumeration early -- not an error, list_dir() still returns OK.
  virtual StorageError list_dir(const char *path, bool (*callback)(const FileStat *entry, void *ctx), void *ctx) = 0;
  virtual StorageError mkdir(const char *path) = 0;
  // Non-recursive: must fail with StorageError::NOT_EMPTY if the directory has contents.
  // For recursive delete, use the free remove_recursive() helper below.
  virtual StorageError rmdir(const char *path) = 0;
  virtual StorageError remove(const char *path) = 0;
  virtual StorageError rename(const char *old_path, const char *new_path) = 0;
  // No copy() here -- drivers only need to move bytes within their own device.
  // Cross-device copy (and same-device copy) is provided by the free copy() helper
  // below, built on read_file()/write_file().

 protected:
  // Stores the mount path -- called once by the driver (codegen setter), never at runtime. The
  // caller satisfies the get_mount_path() contract above; it is a config-time invariant validated
  // in the driver's Python codegen, not re-checked here.
  void set_mount_path_(const char *path) { this->mount_path_ = path; }

  const char *mount_path_{nullptr};
};

// Path-based file access with a local filesystem layer (SD, USB, LittleFS partition). Also the
// right base for stateful network protocols using handles/locks (e.g. SMB); NetworkStorage below is
// stateless only.
class FilesystemStorage : public PathStorage {
 public:
  StorageType get_storage_type() const override { return StorageType::FILESYSTEM; }
  FilesystemStorage *as_filesystem() override { return this; }

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

// Path-based file access over a stateless network protocol (NFS) -- no file handles, no
// connection-held locks. Stateful protocols (SMB) belong under FilesystemStorage, not here.
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
  // Sets `path`'s length to `size`, creating it if absent; growing zero-fills the added range,
  // shrinking discards the tail.
  //
  // Needed because write_chunk() is offset-addressed and never shortens: a short file written over
  // a longer one leaves the old tail in place. FilesystemStorage gets this from OpenMode::WRITE; a
  // stateless protocol has no open() to carry it. copy() calls it with 0 before the first chunk of
  // a file whose destination is a NetworkStorage.
  virtual StorageError truncate(const char *path, uint64_t size) = 0;
};

// Runtime registry of all storage devices. Initializes before any driver (setup_priority::BUS);
// sized exactly at codegen from the configured device count -- no compile-time upper bound.
//
// Contract -- main-loop-only: register_storage()/unregister_storage()/for_each*() must never run
// from another task (a USB hotplug callback defers onto the main loop). "Registered" means
// "usable": register once the device can serve calls (even if unmounted, per get_info()),
// unregister as soon as it stops being usable, before teardown.
//
// Contract: unregister_storage() does not return until every subscriber has stopped using the
// storage -- in particular the worker (storage_worker.h, USE_STORAGE_WORKER) drains in-flight and
// queued data-plane calls against it (best effort: gives up after a bounded timeout, logging an
// error, since the medium is presumed gone). This makes "unregister, then unmount" provably safe:
// on return no async consumer holds an open handle or is mid-call against the storage.
class StorageRegistry : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }

  // Called by codegen with the exact number of configured storage devices
  void set_device_count(size_t count) { this->storages_.init(count); }

  // Guard-rail for the BLOCKING helpers below (read_file/write_file/copy/move): transfers larger
  // than this are rejected with TRANSFER_TOO_LARGE instead of freezing the node -- the bulk paths
  // are the worker's. They hold the whole payload in RAM, so the ceiling stops an automation from
  // asking for a file whose size it never chose. Codegen sets it from YAML; 0 disables the check
  // (only sane when every caller bounds its own sizes).
  void set_max_blocking_transfer_size(uint64_t size) { this->max_blocking_transfer_size_ = size; }
  uint64_t get_max_blocking_transfer_size() const { return this->max_blocking_transfer_size_; }

  // What to do when a same-storage rename() is refused as NOT_SUPPORTED -- an NFS export can span
  // several server file systems and RENAME never crosses one, so a move inside one mount can come
  // back "not this way". On (default): redo as copy + remove (slower, but the move happens). Off:
  // report the refusal as-is, for setups that would rather see the error than a silent full copy.
  void set_move_fallback_copy(bool enable) { this->move_fallback_copy_ = enable; }
  bool get_move_fallback_copy() const { return this->move_fallback_copy_; }

  // OK on success (idempotent: re-registering is OK), INVALID_ARGS for nullptr, NO_SPACE at the
  // codegen-sized capacity -- a codegen/runtime device-count mismatch drivers should treat as fatal
  // (log + mark_failed()) rather than run with an invisibly missing device.
  StorageError register_storage(Storage *s);
  void unregister_storage(Storage *s);
  // Drain-only variant of unregister_storage() for drivers whose registration is permanent but
  // whose medium comes and goes (SD safe-eject, NFS unmount): fires the same drain callbacks -- on
  // return no in-flight data-plane call against `s` remains and pending worker requests are
  // completed with NOT_READY -- but the entry stays registered. The driver flips its mounted state
  // right after; later data-plane calls fail NOT_READY at the driver, so staying registered is
  // safe. No-op if `s` is not registered. Avoids the unregister/re-register churn (log noise,
  // consumers briefly seeing the device vanish).
  void quiesce_storage(Storage *s);
  bool is_registered(const Storage *s) const;

  // Enumeration by index, for callers that cannot use the for_each* callbacks (those run to
  // completion; work spread over several main-loop passes needs its own cursor).
  //
  // An index is a POSITION, never a device: unregister_storage() fills the freed slot with the last
  // entry, so any index at or after the removed one may point elsewhere afterwards and size()
  // shrinks. An index held across register/unregister is stale -- re-derive it, or hold the
  // Storage* and ask is_registered(). get() returns nullptr out of range so a stale cursor cannot
  // read past the end.
  size_t size() const { return this->storages_.size(); }
  Storage *get(size_t index) const { return index < this->storages_.size() ? this->storages_[index] : nullptr; }

  // Enumerate by type -- callback receives each matching device and caller ctx.
  //
  // Each walks a snapshot taken at call start, so a callback may register/unregister without
  // disturbing the walk (nothing skipped, repeated, or read past end). The set is fixed at entry:
  // a storage registered inside a callback is not visited until the next call; one unregistered
  // inside a callback is still visited if not yet reached. The pointer stays valid (ESPHome
  // components are static, never destroyed); callers who must not act on a departed device check
  // is_registered() in the callback.
  void for_each(void (*cb)(Storage *s, void *ctx), void *ctx);
  void for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx);
  void for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx);
  void for_each_kv(void (*cb)(KeyValueStorage *s, void *ctx), void *ctx);
  void for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx);
  // Both FILESYSTEM and NETWORK expose PathStorage -- use this to browse/operate on any path-based
  // storage, local or network-backed.
  void for_each_path_based(void (*cb)(PathStorage *s, void *ctx), void *ctx);
  // Typed overload: also hands the StorageType to the callback, sparing the per-entry virtual
  // get_storage_type() call when distinguishing local vs. network.
  void for_each_path_based(void (*cb)(PathStorage *s, StorageType type, void *ctx), void *ctx);

  // Longest-prefix match of vfs_path against every registered PathStorage's mount point. Matches
  // only at a '/' boundary or exactly (e.g. "/sd2/x" does NOT match mount "/sd"), so prefixes can't
  // shadow each other. nullptr (*rel_out untouched) if none match. On a match *rel_out points into
  // vfs_path: "" if vfs_path IS the mount point, else the remainder starting with '/'.
  PathStorage *resolve_path(const char *vfs_path, const char **rel_out);

  // Canonical VFS path = mount point + `rel`, normalizing the join to exactly one '/' regardless of
  // whether `rel` starts with '/'. Writes into `out` (size `len`); returns false (out unspecified)
  // if the result wouldn't fit.
  static bool build_path(const PathStorage *s, const char *rel, char *out, size_t len);

  // Notification callbacks -- fired when a device registers or unregisters. Templatized so both
  // std::function and pointer-sized forwarder structs are accepted without forcing heap allocation.
  template<typename F> void add_on_registered_callback(F &&cb) { this->on_registered_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_unregistered_callback(F &&cb) { this->on_unregistered_.add(std::forward<F>(cb)); }
  // Fired when a storage stops being usable, whether or not it leaves the registry:
  // quiesce_storage() fires this alone, unregister_storage() fires it then the callback above.
  // Subscribe here for "stop touching this device now" (the worker does, to cancel/drain in-flight
  // requests), above for "this device is gone". Kept apart so a driver can unmount removable media
  // without every consumer seeing a departure never followed by a re-registration.
  template<typename F> void add_on_quiesce_callback(F &&cb) { this->on_quiesce_.add(std::forward<F>(cb)); }

#ifdef USE_STORAGE_CHANGE_FEED
  // Directory-change feed (main loop only). Whoever alters a directory's *listing* notes it here:
  // the worker for every completed transfer (YAML automations included), the web file/raw APIs for
  // their direct operations. "" marks the roots level (a mount came/went). Consumers poll
  // change_seq() with their own cursor and read newer entries -- nothing is cleared, so any number
  // coexist. Small ring: a cursor older than the oldest retained entry has missed evictions and
  // must treat everything as dirty (see web_server file_api's /files/changes, the only consumer
  // today). Bursts into one directory (a tree landing file after file) coalesce into one entry.
  static constexpr size_t DIR_CHANGES_SIZE = 8;
  struct DirChange {
    uint32_t seq{0};  // 0 = slot never used
    std::string dir;
  };
  void note_dir_changed(const std::string &dir);
  // Notes the parent directory of `path` ("" -- the roots level -- for a top-level path).
  void note_parent_changed(const std::string &path);
  uint32_t change_seq() const { return this->change_seq_; }
  const DirChange &dir_change(size_t index) const { return this->dir_changes_[index]; }
#endif

 protected:
  // Copies the currently registered entries into `out` (which must hold STORAGE_MAX_DEVICES
  // pointers) and returns how many were written -- the snapshot the for_each* walkers iterate.
  size_t snapshot_(Storage **out) const;

  // Single allocation at set_device_count() -- no realloc machinery
  FixedVector<Storage *> storages_;
  // Guards storages_ against the one cross-thread pattern: the worker task reads via is_registered()
  // (per-chunk cancellation check) while the main loop mutates via register/unregister. All other
  // accessors (for_each*, size/get, resolve_path) are main-loop-only and need no lock.
  mutable Mutex registry_lock_;

  // LazyCallbackManager: 4-byte nullptr until first subscriber -- saves RAM where nothing listens
  // for hotplug events.
  LazyCallbackManager<void(Storage *)> on_registered_;
  LazyCallbackManager<void(Storage *)> on_unregistered_;
  LazyCallbackManager<void(Storage *)> on_quiesce_;

  uint64_t max_blocking_transfer_size_{0};  // 0 = unlimited
  bool move_fallback_copy_{true};

#ifdef USE_STORAGE_CHANGE_FEED
  DirChange dir_changes_[DIR_CHANGES_SIZE]{};
  size_t dir_changes_next_{0};
  uint32_t change_seq_{0};
#endif
};

extern StorageRegistry *global_storage_registry;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

//========================================================================
// Free helper functions -- convenience wrappers over the Storage interfaces.
// None of these are virtual: drivers never implement or override them.
//========================================================================

// Human-readable name for a StorageError -- use this in dump_config()/logging
// instead of hand-rolling a switch per driver.
const char *error_to_string(StorageError error);

// Maps a POSIX errno to a StorageError, for drivers reaching their medium through the VFS (SD,
// USB). Collapsing every failure into WRITE_ERROR loses what a caller needs: "destination exists"
// (EEXIST) differs from "source is gone" (ENOENT). `writing` picks the fallback direction for
// unmapped errnos. Call only after a real failure -- `err` must be a real errno, never 0 (a zero
// is not success; it hits the same fallback as any unmapped value).
StorageError error_from_errno(int err, bool writing);

// stat()-based existence/size checks -- thin wrappers over any PathStorage.
// exists(): only NOT_FOUND is a clean "no". Any other non-OK (NOT_READY, READ_ERROR,
// PERMISSION_DENIED, ...) also returns false but is surfaced via err_out, so callers can tell
// "file absent" from "medium unmounted/faulted" before deciding to create/overwrite.
bool exists(PathStorage *storage, const char *path, StorageError *err_out);
StorageError file_size(PathStorage *storage, const char *path, uint64_t *size);

// Frees memory from RAMAllocator<uint8_t> (malloc/heap_caps_malloc_prefer), not operator delete[]
// -- required because RamBuffer below is backed by RAMAllocator, not `new[]`.
struct RamBufferDeleter {
  size_t size{0};  // a default-constructed RamBuffer must carry a well-defined (unused) size
  void operator()(uint8_t *ptr) const {
    if (ptr != nullptr)
      RAMAllocator<uint8_t>().deallocate(ptr, size);
  }
};
using RamBuffer = std::unique_ptr<uint8_t[], RamBufferDeleter>;

// Allocates a streaming buffer whose size and placement follow the execution context (20 ms-budget
// analysis):
//   - loop path (on_task = false) and every non-PSRAM-DMA chip: `want` bytes in internal DMA-capable
//     RAM, halving to a 4 kB floor under pressure. 16 kB default (the most a slow SD write clears in
//     one 20 ms loop slice).
//   - worker task (on_task = true) on S3/P4 (PSRAM is DMA-capable): 64 kB (P4) / 32 kB (S3) in
//     MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, falling back to the internal path at `want` if PSRAM is
//     tight. No 20 ms budget on the task, so a larger chunk cuts I/O calls and DMAs from PSRAM.
// *actual_size holds the size obtained. Frees through RamBufferDeleter (free() == heap_caps_free()
// on ESP32). Null RamBuffer if even 4 kB cannot be met.
RamBuffer alloc_dma_capable(size_t want, bool on_task, size_t *actual_size);

// Reads an entire file in one call. Allocates via RAMAllocator (nothrow -- NO_SPACE on failure, not
// throw/abort, since ESPHome builds without exceptions). Not for hot paths or post-setup() on the
// main loop; for occasional whole-file reads (e.g. serving over HTTP). On success *out owns the
// buffer and *size holds the byte count.
StorageError read_file(FilesystemStorage *storage, const char *path, RamBuffer &out, size_t *size);
StorageError read_file(NetworkStorage *storage, const char *path, RamBuffer &out, size_t *size);
// PathStorage overload: dispatches on get_storage_type(), so a generic path-based consumer (e.g. a
// file browser holding a PathStorage*) calls read_file() without downcasting first.
StorageError read_file(PathStorage *storage, const char *path, RamBuffer &out, size_t *size);

// Writes an entire buffer to a file in one call (create/truncate semantics, like
// OpenMode::WRITE).
StorageError write_file(FilesystemStorage *storage, const char *path, const uint8_t *data, size_t size);
StorageError write_file(NetworkStorage *storage, const char *path, const uint8_t *data, size_t size);
// PathStorage overload -- see read_file(PathStorage *, ...) above.
StorageError write_file(PathStorage *storage, const char *path, const uint8_t *data, size_t size);

// Appends a buffer to a file in one call, creating it if absent. Filesystem storages use a native
// OpenMode::APPEND; network storages stat for the current size and write_chunk() at that offset
// (O(1) RAM, no read-modify-write -- the stat->write window is not atomic against other writers,
// acceptable for a single node appending its own logs). Same blocking-size limit and short-write
// contract as write_file().
StorageError append_file(FilesystemStorage *storage, const char *path, const uint8_t *data, size_t size);
StorageError append_file(NetworkStorage *storage, const char *path, const uint8_t *data, size_t size);
// PathStorage overload -- see read_file(PathStorage *, ...) above.
StorageError append_file(PathStorage *storage, const char *path, const uint8_t *data, size_t size);

// Copies a file OR a whole directory tree, same storage or across two (e.g. SD -> USB, USB -> NFS)
// -- the source decides which, recursion included (bounded by STORAGE_MAX_RECURSION_DEPTH, missing
// destination directories created on the way). Dispatches on get_storage_type() and streams every
// file through one fixed STORAGE_COPY_CHUNK_SIZE buffer, reused across a tree rather than allocated
// per entry, never holding a whole file in RAM, feeding the watchdog between chunks and entries. It
// still BLOCKS the main loop, and max_blocking_transfer_size is checked per file, not per tree (a
// tree of many small files passes it and can still take a while). To not block, use the worker
// (StorageWorker::async_copy()/async_copy_tree()).
// No rollback on failure: a partial file or tree keeps whatever landed, the source is never
// touched, all-or-nothing is the caller's to clean up.
// "Created on the way" means each directory the walk descends into is created, but the
// destination's OWN parents are not: copying into /usb/a/b when /usb/a does not exist fails, like
// cp, rather than inventing a path. The worker creates the destination root and each level below
// it, no more, so both paths answer alike.
StorageError copy(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path);

// Moves a file or a whole tree, same storage or across two. Same-storage moves go through rename()
// directly -- no chunk buffer, no size limit, near-O(1), a tree costs the same as a file; if the
// driver refuses with NOT_SUPPORTED, falls back to copy + remove unless move_fallback_copy is off.
// Cross-storage moves go through copy() (inheriting its chunking, watchdog feeding, and size limit)
// then remove() on the source, but ONLY if the copy succeeded. If the source remove() fails, its
// error is returned and the destination copy is kept -- the file ends up in both places, not
// neither.
StorageError move(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path);

// Recursively removes a directory and everything under it, via list_dir() + remove() /
// remove_recursive() for subdirs + a final non-recursive rmdir(). Drivers implement only the
// non-recursive rmdir() primitive. Aborts with INVALID_ARGS if nested deeper than
// STORAGE_MAX_RECURSION_DEPTH.
StorageError remove_recursive(PathStorage *storage, const char *path);

}  // namespace esphome::storage
