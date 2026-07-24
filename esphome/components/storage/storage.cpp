#include "storage.h"

#include <cerrno>
#include <cstring>
#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::storage {

static const char *const TAG = "storage";

StorageRegistry *global_storage_registry = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_STORAGE_CHANGE_FEED
// Collapse duplicate slashes and drop any trailing slash, so a directory is reported to the
// change feed in exactly the form the browser holds it as an openDirs key (the same shape the
// file API's normalize_vfs_path produces). Callers assemble paths as mount_path + "/" + rel,
// which can introduce "//" (empty rel, or a rel with a leading slash) or a trailing slash -- an
// un-normalized string would never string-match the client's key and the relist would be lost.
// "" (the roots marker) passes through unchanged.
static std::string normalize_feed_dir(const std::string &dir) {
  std::string out;
  out.reserve(dir.size());
  bool prev_slash = false;
  for (char ch : dir) {
    if (ch == '/') {
      if (prev_slash)
        continue;
      prev_slash = true;
    } else {
      prev_slash = false;
    }
    out += ch;
  }
  while (out.size() > 1 && out.back() == '/')
    out.pop_back();
  return out;
}

void StorageRegistry::note_dir_changed(const std::string &dir_raw) {
  const std::string dir = normalize_feed_dir(dir_raw);
  // Coalesce bursts into the same directory: bumping the newest entry's seq is enough -- a
  // client behind it is handed the dir exactly once either way.
  size_t newest = (this->dir_changes_next_ + DIR_CHANGES_SIZE - 1) % DIR_CHANGES_SIZE;
  if (this->dir_changes_[newest].seq != 0 && this->dir_changes_[newest].dir == dir) {
    this->dir_changes_[newest].seq = ++this->change_seq_;
    return;
  }
  auto &e = this->dir_changes_[this->dir_changes_next_];
  this->dir_changes_next_ = (this->dir_changes_next_ + 1) % DIR_CHANGES_SIZE;
  e.seq = ++this->change_seq_;
  e.dir = dir;
}

void StorageRegistry::note_parent_changed(const std::string &path_raw) {
  // Normalize first so a trailing or doubled slash cannot split the parent at the wrong place
  // (e.g. "/sd//foo/" must yield "/sd", not "/sd//foo"). note_dir_changed normalizes again,
  // which is harmless.
  const std::string path = normalize_feed_dir(path_raw);
  size_t slash = path.rfind('/');
  // A top-level path's parent is the roots level itself -- the feed's "" marker.
  this->note_dir_changed(slash == std::string::npos || slash == 0 ? std::string() : path.substr(0, slash));
}
#endif

StorageError StorageRegistry::register_storage(Storage *s) {
  if (s == nullptr)
    return StorageError::INVALID_ARGS;

  {
    // Mutation guarded against the worker task's concurrent is_registered() reads.
    LockGuard guard(this->registry_lock_);
    for (auto *existing : this->storages_) {
      if (existing == s)
        return StorageError::OK;  // already registered -- idempotent
    }

    if (this->storages_.full()) {
      // Codegen sizes the registry to the exact configured device count, so hitting this
      // means a codegen/runtime mismatch -- surface it so the driver can fail loudly.
      ESP_LOGE(TAG, "Registry full -- increase device count");
      return StorageError::NO_SPACE;
    }
    this->storages_.push_back(s);
  }

  // Log regardless of get_info()'s result -- an unmounted-but-registered device (e.g. before
  // its medium is mounted) must still show up, per the get_info() contract on Storage above.
  StorageInfo info{};
  if (s->get_info(&info) == StorageError::OK) {
    ESP_LOGD(TAG, "Storage registered: %s (%s)%s", info.name != nullptr ? info.name : "?",
             info.id != nullptr ? info.id : "?", info.is_mounted ? "" : " [not mounted]");
  } else {
    ESP_LOGD(TAG, "Storage registered (get_info failed)");
  }

  this->on_registered_.call(s);
  return StorageError::OK;
}

void StorageRegistry::quiesce_storage(Storage *s) {
  if (s == nullptr)
    return;
  {
    // Only drain for entries that are actually registered -- the callbacks' contract is
    // "stop using this registered storage now".
    LockGuard guard(this->registry_lock_);
    bool found = false;
    for (Storage *registered : this->storages_) {
      if (registered == s) {
        found = true;
        break;
      }
    }
    if (!found)
      return;
  }
  // Same drain as unregister_storage() (see the comment there on why this must run while
  // the entry is still registered), just without the removal afterwards. Only the drain
  // channel fires: the device stays registered and usable once its medium is back, so
  // telling on_unregistered_ subscribers it went away would be a departure they never see
  // a matching registration for.
  this->on_quiesce_.call(s);
}

void StorageRegistry::unregister_storage(Storage *s) {
  if (s == nullptr)
    return;

  {
    LockGuard guard(this->registry_lock_);
    bool found = false;
    for (auto *registered : this->storages_) {
      if (registered == s) {
        found = true;
        break;
      }
    }
    if (!found)
      return;
  }

  // Drain FIRST, while the entry is still registered: the worker's drain handler
  // synchronously cancels/waits out any in-flight task work on this storage, and the task's
  // per-chunk is_registered() checks must be able to run against an intact vector until that
  // drain completes. Only then is it safe to mutate storages_ below. (The lock alone is not
  // enough: removing before the drain would free this storage's slot while the task may
  // still be inside a blocking I/O call on it.)
  //
  // Removal implies quiescing, so both channels fire, drain first. Both run before the entry
  // is taken out, which is the timing on_unregistered_ subscribers already had.
  this->on_quiesce_.call(s);
  this->on_unregistered_.call(s);

  {
    LockGuard guard(this->registry_lock_);
    // Swap-remove: no reallocation, keeps capacity -- safe since order doesn't matter here
    // (for_each* enumerate every entry regardless of position). Re-locate the entry: the
    // drain callback above runs arbitrary consumer code that could itself have mutated the
    // registry.
    size_t found_index = this->storages_.size();
    for (size_t i = 0; i < this->storages_.size(); i++) {
      if (this->storages_[i] == s) {
        found_index = i;
        break;
      }
    }
    if (found_index == this->storages_.size())
      return;

    size_t last_index = this->storages_.size() - 1;
    if (found_index != last_index)
      this->storages_[found_index] = this->storages_[last_index];
    this->storages_.pop_back();
  }

  StorageInfo info{};
  if (s->get_info(&info) == StorageError::OK) {
    ESP_LOGD(TAG, "Storage unregistered: %s", info.name != nullptr ? info.name : "?");
  }
}

bool StorageRegistry::is_registered(const Storage *s) const {
  // Called from the worker task (per-chunk cancellation check) concurrently with main-loop
  // register/unregister mutations -- hence the lock. Held only for the short scan.
  LockGuard guard(this->registry_lock_);
  for (auto *registered : this->storages_) {
    if (registered == s) {
      return true;
    }
  }
  return false;
}

size_t StorageRegistry::snapshot_(Storage **out) const {
  size_t n = 0;
  for (auto *s : this->storages_) {
    if (n >= STORAGE_MAX_DEVICES)
      break;  // codegen sizes both to device_count, so this cannot trip in a real build
    out[n++] = s;
  }
  return n;
}

void StorageRegistry::for_each(void (*cb)(Storage *s, void *ctx), void *ctx) {
  Storage *entries[STORAGE_MAX_DEVICES];
  size_t n = this->snapshot_(entries);
  for (size_t i = 0; i < n; i++) {
    cb(entries[i], ctx);
  }
}

void StorageRegistry::for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx) {
  Storage *entries[STORAGE_MAX_DEVICES];
  size_t n = this->snapshot_(entries);
  for (size_t i = 0; i < n; i++) {
    if (entries[i]->get_storage_type() == StorageType::FILESYSTEM)
      cb(static_cast<FilesystemStorage *>(entries[i]), ctx);
  }
}

void StorageRegistry::for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx) {
  Storage *entries[STORAGE_MAX_DEVICES];
  size_t n = this->snapshot_(entries);
  for (size_t i = 0; i < n; i++) {
    if (entries[i]->get_storage_type() == StorageType::RAW)
      cb(static_cast<RawStorage *>(entries[i]), ctx);
  }
}

void StorageRegistry::for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx) {
  Storage *entries[STORAGE_MAX_DEVICES];
  size_t n = this->snapshot_(entries);
  for (size_t i = 0; i < n; i++) {
    if (entries[i]->get_storage_type() == StorageType::NETWORK)
      cb(static_cast<NetworkStorage *>(entries[i]), ctx);
  }
}

void StorageRegistry::for_each_path_based(void (*cb)(PathStorage *s, void *ctx), void *ctx) {
  Storage *entries[STORAGE_MAX_DEVICES];
  size_t n = this->snapshot_(entries);
  for (size_t i = 0; i < n; i++) {
    StorageType type = entries[i]->get_storage_type();
    if (type == StorageType::FILESYSTEM) {
      cb(static_cast<FilesystemStorage *>(entries[i]), ctx);
    } else if (type == StorageType::NETWORK) {
      cb(static_cast<NetworkStorage *>(entries[i]), ctx);
    }
  }
}

void StorageRegistry::for_each_path_based(void (*cb)(PathStorage *s, StorageType type, void *ctx), void *ctx) {
  Storage *entries[STORAGE_MAX_DEVICES];
  size_t n = this->snapshot_(entries);
  for (size_t i = 0; i < n; i++) {
    StorageType type = entries[i]->get_storage_type();
    if (type == StorageType::FILESYSTEM) {
      cb(static_cast<FilesystemStorage *>(entries[i]), type, ctx);
    } else if (type == StorageType::NETWORK) {
      cb(static_cast<NetworkStorage *>(entries[i]), type, ctx);
    }
  }
}

PathStorage *StorageRegistry::resolve_path(const char *vfs_path, const char **rel_out) {
  StringRef path_ref(vfs_path);
  PathStorage *best = nullptr;
  size_t best_len = 0;

  for (auto *s : this->storages_) {
    StorageType type = s->get_storage_type();
    PathStorage *ps;
    if (type == StorageType::FILESYSTEM) {
      ps = static_cast<FilesystemStorage *>(s);
    } else if (type == StorageType::NETWORK) {
      ps = static_cast<NetworkStorage *>(s);
    } else {
      continue;
    }
    // No null check: validate_mount_path() in the driver's codegen guarantees one.
    StringRef mount_ref(ps->get_mount_path());
    size_t mount_len = mount_ref.size();

    // Prefix match only at a '/' boundary or an exact match -- "/sd2/x" must not match "/sd".
    if (path_ref.size() < mount_len)
      continue;
    StringRef path_prefix(vfs_path, mount_len);
    if (path_prefix.compare(mount_ref) != 0)
      continue;
    if (path_ref.size() > mount_len && path_ref[mount_len] != '/')
      continue;

    // Longest match wins, so a more specific mount point (e.g. "/sd/nested") takes priority
    // over a shorter one that's also a valid prefix (e.g. "/sd").
    if (mount_len > best_len) {
      best = ps;
      best_len = mount_len;
    }
  }

  if (best == nullptr)
    return nullptr;
  *rel_out = vfs_path + best_len;  // "" if vfs_path == mount point exactly, else starts with '/'
  return best;
}

bool StorageRegistry::build_path(const PathStorage *s, const char *rel, char *out, size_t len) {
  StringRef mount_ref(s->get_mount_path());
  // A rel of "/" names the mount point itself, exactly like "", and must not leave a trailing
  // separator behind: resolve_path() hands back "" for that case and this is its inverse.
  StringRef rel_ref((rel[0] == '/' && rel[1] == '\0') ? "" : rel);
  bool rel_has_slash = !rel_ref.empty() && rel_ref[0] == '/';

  size_t total = mount_ref.size() + (rel_has_slash || rel_ref.empty() ? 0 : 1) + rel_ref.size() + 1;
  if (total > len)
    return false;

  size_t pos = mount_ref.copy(out, mount_ref.size());
  if (!rel_has_slash && !rel_ref.empty())
    out[pos++] = '/';
  pos += rel_ref.copy(out + pos, rel_ref.size());
  out[pos] = '\0';
  return true;
}

//========================================================================
// Free helper functions
//========================================================================

// Shared guard-rail check for the blocking helpers below. No-op (returns OK) when no limit is
// configured (registry unset, or max_blocking_transfer_size == 0).
static StorageError check_blocking_transfer_size(uint64_t size) {
  if (global_storage_registry == nullptr)
    return StorageError::OK;
  uint64_t limit = global_storage_registry->get_max_blocking_transfer_size();
  if (limit != 0 && size > limit)
    return StorageError::TRANSFER_TOO_LARGE;
  return StorageError::OK;
}

StorageError error_from_errno(int err, bool writing) {
  switch (err) {
    case ENOENT:
      return StorageError::NOT_FOUND;
    case EEXIST:
      return StorageError::ALREADY_EXISTS;
    case ENOTEMPTY:
      return StorageError::NOT_EMPTY;
    case ENOSPC:
      return StorageError::NO_SPACE;
    case EACCES:
    case EPERM:
    case EROFS:
      return StorageError::PERMISSION_DENIED;
    case EMFILE:
    case ENFILE:
      return StorageError::TOO_MANY_OPEN_FILES;
    case EINVAL:
    case ENAMETOOLONG:
    case EISDIR:   // asked for a file, found a directory
    case ENOTDIR:  // ... or the other way round (littlefs reports both)
      return StorageError::INVALID_ARGS;
    case ENOTSUP:
    case EXDEV:  // rename() across volumes -- the caller must copy instead
      return StorageError::NOT_SUPPORTED;
    case ENODEV:
      return StorageError::NOT_READY;
    case ETIMEDOUT:
      return StorageError::TIMEOUT;
    case EILSEQ:
      return StorageError::CORRUPT;
    default:
      return writing ? StorageError::WRITE_ERROR : StorageError::READ_ERROR;
  }
}

const char *error_to_string(StorageError error) {
  switch (error) {
    case StorageError::OK:
      return "OK";
    case StorageError::NOT_READY:
      return "NOT_READY";
    case StorageError::READ_ERROR:
      return "READ_ERROR";
    case StorageError::WRITE_ERROR:
      return "WRITE_ERROR";
    case StorageError::INVALID_ARGS:
      return "INVALID_ARGS";
    case StorageError::NOT_FOUND:
      return "NOT_FOUND";
    case StorageError::NO_SPACE:
      return "NO_SPACE";
    case StorageError::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case StorageError::TIMEOUT:
      return "TIMEOUT";
    case StorageError::CORRUPT:
      return "CORRUPT";
    case StorageError::NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case StorageError::ALREADY_EXISTS:
      return "ALREADY_EXISTS";
    case StorageError::NOT_EMPTY:
      return "NOT_EMPTY";
    case StorageError::TOO_MANY_OPEN_FILES:
      return "TOO_MANY_OPEN_FILES";
    case StorageError::TRANSFER_TOO_LARGE:
      return "TRANSFER_TOO_LARGE";
    case StorageError::VERIFY_MISMATCH:
      return "VERIFY_MISMATCH";
  }
  return "UNKNOWN";
}

bool exists(PathStorage *storage, const char *path, StorageError *err_out) {
  FileStat stat{};
  StorageError err = storage->stat(path, &stat);
  if (err_out != nullptr)
    *err_out = err;
  // Only NOT_FOUND is a clean "no"; other errors also yield false but are reported via
  // err_out -- see the header comment (a transient NOT_READY must not look like absence).
  return err == StorageError::OK;
}

StorageError file_size(PathStorage *storage, const char *path, uint64_t *size) {
  FileStat stat{};
  StorageError err = storage->stat(path, &stat);
  if (err != StorageError::OK)
    return err;
  *size = stat.size;
  return StorageError::OK;
}

StorageError read_file(FilesystemStorage *storage, const char *path, RamBuffer &out, size_t *size) {
  FileStat stat{};
  StorageError err = storage->stat(path, &stat);
  if (err != StorageError::OK)
    return err;
  err = check_blocking_transfer_size(stat.size);
  if (err != StorageError::OK)
    return err;
  // stat.size is uint64_t (NetworkStorage can see files >4GB); reject anything that wouldn't
  // fit in a size_t before it gets silently truncated by the allocation below. Guarded so the
  // comparison is not tautological where size_t is already 64 bit (host builds, unit tests).
  if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
    if (stat.size > static_cast<uint64_t>(SIZE_MAX))
      return StorageError::NO_SPACE;
  }
  auto buf_size = static_cast<size_t>(stat.size);

  // Empty file: legitimate result, not an allocation failure. allocate(0) may return nullptr,
  // which the check below would misreport as NO_SPACE -- return an empty buffer instead.
  if (buf_size == 0) {
    out = RamBuffer(nullptr, RamBufferDeleter{0});
    *size = 0;
    return StorageError::OK;
  }

  uint8_t *raw = RAMAllocator<uint8_t>().allocate(buf_size);
  if (raw == nullptr)
    return StorageError::NO_SPACE;
  RamBuffer buf(raw, RamBufferDeleter{buf_size});

  FileHandle *handle = nullptr;
  err = storage->open(path, handle, OpenMode::READ);
  if (err != StorageError::OK)
    return err;

  size_t total_read = 0;
  while (total_read < buf_size) {
    size_t bytes_transferred = 0;
    err = storage->read(handle, buf.get() + total_read, buf_size - total_read, &bytes_transferred);
    if (err != StorageError::OK) {
      storage->close(handle);
      return err;
    }
    if (bytes_transferred == 0)
      break;  // EOF before stat()-reported size -- return what we got
    total_read += bytes_transferred;
    App.feed_wdt();
  }
  storage->close(handle);

  out = std::move(buf);
  *size = total_read;
  return StorageError::OK;
}

StorageError read_file(NetworkStorage *storage, const char *path, RamBuffer &out, size_t *size) {
  FileStat stat{};
  StorageError err = storage->stat(path, &stat);
  if (err != StorageError::OK)
    return err;
  err = check_blocking_transfer_size(stat.size);
  if (err != StorageError::OK)
    return err;
  // stat.size is uint64_t (NetworkStorage can see files >4GB); reject anything that wouldn't
  // fit in a size_t before it gets silently truncated by the allocation below. Guarded so the
  // comparison is not tautological where size_t is already 64 bit (host builds, unit tests).
  if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
    if (stat.size > static_cast<uint64_t>(SIZE_MAX))
      return StorageError::NO_SPACE;
  }
  auto buf_size = static_cast<size_t>(stat.size);

  // Empty file: legitimate result, not an allocation failure. allocate(0) may return nullptr,
  // which the check below would misreport as NO_SPACE -- return an empty buffer instead.
  if (buf_size == 0) {
    out = RamBuffer(nullptr, RamBufferDeleter{0});
    *size = 0;
    return StorageError::OK;
  }

  uint8_t *raw = RAMAllocator<uint8_t>().allocate(buf_size);
  if (raw == nullptr)
    return StorageError::NO_SPACE;
  RamBuffer buf(raw, RamBufferDeleter{buf_size});

  size_t total_read = 0;
  while (total_read < buf_size) {
    size_t bytes_transferred = 0;
    err = storage->read_chunk(path, buf.get() + total_read, total_read, buf_size - total_read, &bytes_transferred);
    if (err != StorageError::OK)
      return err;
    if (bytes_transferred == 0)
      break;  // EOF before stat()-reported size -- return what we got
    total_read += bytes_transferred;
    App.feed_wdt();
  }

  out = std::move(buf);
  *size = total_read;
  return StorageError::OK;
}

StorageError write_file(FilesystemStorage *storage, const char *path, const uint8_t *data, size_t size) {
  StorageError err = check_blocking_transfer_size(size);
  if (err != StorageError::OK)
    return err;

  FileHandle *handle = nullptr;
  err = storage->open(path, handle, OpenMode::WRITE);
  if (err != StorageError::OK)
    return err;

  size_t total_written = 0;
  while (total_written < size) {
    size_t bytes_transferred = 0;
    err = storage->write(handle, data + total_written, size - total_written, &bytes_transferred);
    if (err != StorageError::OK) {
      storage->close(handle);
      return err;
    }
    if (bytes_transferred == 0) {
      storage->close(handle);
      return StorageError::WRITE_ERROR;
    }
    total_written += bytes_transferred;
    App.feed_wdt();
  }
  return storage->close(handle);
}

StorageError write_file(NetworkStorage *storage, const char *path, const uint8_t *data, size_t size) {
  StorageError size_check = check_blocking_transfer_size(size);
  if (size_check != StorageError::OK)
    return size_check;

  // Create/truncate semantics, same as the FilesystemStorage overload's OpenMode::WRITE.
  // write_chunk() addresses by offset and never shortens, so without this a shorter payload
  // would leave the previous file's tail in place.
  StorageError trunc_err = storage->truncate(path, 0);
  if (trunc_err != StorageError::OK)
    return trunc_err;

  size_t total_written = 0;
  while (total_written < size) {
    size_t bytes_transferred = 0;
    StorageError err =
        storage->write_chunk(path, data + total_written, total_written, size - total_written, &bytes_transferred);
    if (err != StorageError::OK)
      return err;
    if (bytes_transferred == 0)
      return StorageError::WRITE_ERROR;
    total_written += bytes_transferred;
    App.feed_wdt();
  }
  return StorageError::OK;
}

StorageError read_file(PathStorage *storage, const char *path, RamBuffer &out, size_t *size) {
  switch (storage->get_storage_type()) {
    case StorageType::FILESYSTEM:
      return read_file(static_cast<FilesystemStorage *>(storage), path, out, size);
    case StorageType::NETWORK:
      return read_file(static_cast<NetworkStorage *>(storage), path, out, size);
    default:
      return StorageError::NOT_SUPPORTED;
  }
}

StorageError write_file(PathStorage *storage, const char *path, const uint8_t *data, size_t size) {
  switch (storage->get_storage_type()) {
    case StorageType::FILESYSTEM:
      return write_file(static_cast<FilesystemStorage *>(storage), path, data, size);
    case StorageType::NETWORK:
      return write_file(static_cast<NetworkStorage *>(storage), path, data, size);
    default:
      return StorageError::NOT_SUPPORTED;
  }
}

// The bytes of one file, given a chunk buffer to borrow. Split out of copy() so a directory
// walk can reuse the same buffer for every file instead of allocating one per entry.
static StorageError copy_one_file(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage,
                                  const char *dst_path, const RamBuffer &chunk_buf, size_t chunk_size) {
  bool src_is_fs = src_storage->get_storage_type() == StorageType::FILESYSTEM;
  bool dst_is_fs = dst_storage->get_storage_type() == StorageType::FILESYSTEM;
  auto *src_fs = src_is_fs ? static_cast<FilesystemStorage *>(src_storage) : nullptr;
  auto *src_net = src_is_fs ? nullptr : static_cast<NetworkStorage *>(src_storage);
  auto *dst_fs = dst_is_fs ? static_cast<FilesystemStorage *>(dst_storage) : nullptr;
  auto *dst_net = dst_is_fs ? nullptr : static_cast<NetworkStorage *>(dst_storage);

  FileHandle *src_handle = nullptr;
  FileHandle *dst_handle = nullptr;
  // Initialized rather than left to the loop below: that always assigns before reading, but
  // only by control flow, and a network-to-network copy enters neither open() branch first.
  StorageError err = StorageError::OK;
  if (src_is_fs) {
    err = src_fs->open(src_path, src_handle, OpenMode::READ);
    if (err != StorageError::OK)
      return err;
  }
  if (dst_is_fs) {
    err = dst_fs->open(dst_path, dst_handle, OpenMode::WRITE);
    if (err != StorageError::OK) {
      if (src_is_fs)
        src_fs->close(src_handle);
      return err;
    }
  } else {
    // OpenMode::WRITE truncates for the filesystem branch above; write_chunk() does not, so a
    // shorter source would leave the destination's old tail behind.
    err = dst_net->truncate(dst_path, 0);
    if (err != StorageError::OK) {
      if (src_is_fs)
        src_fs->close(src_handle);
      return err;
    }
  }

  uint64_t offset = 0;
  bool done = false;
  while (!done) {
    size_t bytes_read = 0;
    if (src_is_fs) {
      err = src_fs->read(src_handle, chunk_buf.get(), chunk_size, &bytes_read);
    } else {
      err = src_net->read_chunk(src_path, chunk_buf.get(), offset, chunk_size, &bytes_read);
    }
    if (err != StorageError::OK || bytes_read == 0)
      break;  // error, or EOF

    size_t total_written = 0;
    while (total_written < bytes_read && err == StorageError::OK) {
      size_t bytes_written = 0;
      if (dst_is_fs) {
        err = dst_fs->write(dst_handle, chunk_buf.get() + total_written, bytes_read - total_written, &bytes_written);
      } else {
        err = dst_net->write_chunk(dst_path, chunk_buf.get() + total_written, offset + total_written,
                                   bytes_read - total_written, &bytes_written);
      }
      if (err != StorageError::OK) {
        done = true;
      } else if (bytes_written == 0) {
        err = StorageError::WRITE_ERROR;
        done = true;
      } else {
        total_written += bytes_written;
      }
    }
    if (done)
      break;

    offset += bytes_read;
    App.feed_wdt();
  }

  if (src_is_fs)
    src_fs->close(src_handle);  // close result intentionally ignored -- source is read-only
  if (dst_is_fs) {
    // FATFS flushes on close; a failed close on the destination means a silently truncated/
    // corrupt file, so its result must win over an OK from the copy loop above.
    StorageError close_err = dst_fs->close(dst_handle);
    if (err == StorageError::OK)
      err = close_err;
  }
  return err;
}

// Appends "/<name>" to the path already in `buf` at offset `len`, and returns the new length --
// 0 if it would not fit. Both walks below build their paths this way, in one buffer that is
// extended on the way down and truncated on the way back up, so a path costs its bytes once
// instead of once per recursion level.
static size_t append_path_segment(char *buf, size_t len, const char *name) {
  int n = snprintf(buf + len, STORAGE_PATH_MAX - len, "/%s", name);
  if (n < 0 || static_cast<size_t>(n) >= STORAGE_PATH_MAX - len) {
    buf[len] = '\0';  // snprintf already wrote what fit -- leave the caller's path untouched
    return 0;
  }
  return len + static_cast<size_t>(n);
}

struct CopyTreeCtx;
static StorageError copy_tree_at_depth(PathStorage *src_storage, char *src_path, size_t src_len,
                                       PathStorage *dst_storage, char *dst_path, size_t dst_len,
                                       const RamBuffer &chunk_buf, size_t chunk_size, size_t depth);

struct CopyTreeCtx {
  PathStorage *src_storage;
  PathStorage *dst_storage;
  // Buffers owned by copy(), shared by every level; *_len is where this level's directory path
  // ends and a child segment gets appended.
  char *src_path;
  char *dst_path;
  size_t src_len;
  size_t dst_len;
  const RamBuffer &chunk_buf;
  size_t chunk_size;
  size_t depth;
  StorageError err{StorageError::OK};
};

static bool copy_tree_cb(const FileStat *entry, void *ctx_ptr) {
  auto *ctx = static_cast<CopyTreeCtx *>(ctx_ptr);

  size_t src_len = append_path_segment(ctx->src_path, ctx->src_len, entry->name);
  size_t dst_len = append_path_segment(ctx->dst_path, ctx->dst_len, entry->name);
  if (src_len == 0 || dst_len == 0) {
    ctx->err = StorageError::INVALID_ARGS;
    return false;
  }

  StorageError err;
  if (entry->is_dir) {
    err = copy_tree_at_depth(ctx->src_storage, ctx->src_path, src_len, ctx->dst_storage, ctx->dst_path, dst_len,
                             ctx->chunk_buf, ctx->chunk_size, ctx->depth + 1);
  } else {
    err = check_blocking_transfer_size(entry->size);
    if (err == StorageError::OK) {
      err = copy_one_file(ctx->src_storage, ctx->src_path, ctx->dst_storage, ctx->dst_path, ctx->chunk_buf,
                          ctx->chunk_size);
    }
  }
  App.feed_wdt();

  // Back to this level's directory before the next entry is appended.
  ctx->src_path[ctx->src_len] = '\0';
  ctx->dst_path[ctx->dst_len] = '\0';

  if (err != StorageError::OK) {
    ctx->err = err;
    return false;  // stop enumeration -- an entry failed
  }
  return true;
}

static StorageError copy_tree_at_depth(PathStorage *src_storage, char *src_path, size_t src_len,
                                       PathStorage *dst_storage, char *dst_path, size_t dst_len,
                                       const RamBuffer &chunk_buf, size_t chunk_size, size_t depth) {
  if (depth > STORAGE_MAX_RECURSION_DEPTH)
    return StorageError::INVALID_ARGS;

  StorageError err = dst_storage->mkdir(dst_path);
  if (err != StorageError::OK && err != StorageError::ALREADY_EXISTS)
    return err;

  CopyTreeCtx ctx{src_storage, dst_storage, src_path, dst_path, src_len, dst_len, chunk_buf, chunk_size, depth};
  err = src_storage->list_dir(src_path, copy_tree_cb, &ctx);
  if (err != StorageError::OK)
    return err;
  return ctx.err;
}

// Streaming-buffer allocation with placement chosen from the execution context. See the
// declaration in storage.h for the policy; the mechanics live here.
//
// The 20 ms loop-slice budget caps a main-loop chunk at ~16 kB even on the fastest S3 SD path
// (a slow SPI write does not manage more in 20 ms), so the loop path stays at `want` in internal
// RAM. The worker task carries no such budget, so on a chip whose PSRAM can be a DMA target
// (S3/P4) it stages a larger chunk (64 kB on P4, 32 kB on S3) in DMA-capable PSRAM -- fewer I/O
// calls, DMA straight out of the arena. Everywhere else the task path is the same as the loop
// path: `want` internal.
//
// Uses heap_caps_malloc directly (not RAMAllocator) because only the cap-based API can request
// MALLOC_CAP_DMA. The result still frees through RamBufferDeleter: on ESP32 free() routes to the
// owning heap, so a heap_caps_malloc'd block and a RAMAllocator block are freed identically.
RamBuffer alloc_dma_capable(size_t want, bool on_task, size_t *actual_size) {
  uint8_t *raw = nullptr;
#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
  constexpr bool psram_dma = true;
#else
  constexpr bool psram_dma = false;
#endif
  // Task path on a PSRAM-DMA chip: stage a large DMA-capable PSRAM chunk. Sizes match the
  // proven throughput of the previous monolithic component -- 64 kB on P4 (wide SDMMC bus +
  // guaranteed PSRAM), 32 kB on S3. Bigger fread/fwrite blocks cut per-call VFS/FatFs overhead,
  // and the task carries no 20 ms loop budget so the larger block is safe here. On failure fall
  // through to the internal path below at `want` (never leaves the transfer without a buffer).
#if defined(USE_ESP32_VARIANT_ESP32P4)
  constexpr size_t task_psram_chunk = 65536;
#else
  constexpr size_t task_psram_chunk = 32768;
#endif
  if (on_task && psram_dma) {
    raw = static_cast<uint8_t *>(heap_caps_malloc(task_psram_chunk, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA));
    if (raw != nullptr) {
      *actual_size = task_psram_chunk;
      return RamBuffer(raw, RamBufferDeleter{task_psram_chunk});
    }
  }
  // Internal, DMA-capable, halving on memory pressure down to a 4 kB floor. This is the loop
  // path everywhere, the whole story on non-PSRAM-DMA chips, and the fallback when the PSRAM
  // staging allocation above could not be met.
  size_t size = want;
  while (size >= 4096) {
    raw = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (raw != nullptr)
      break;
    size /= 2;
  }
#else
  // Non-ESP32: no heap_caps / no external RAM notion -- plain malloc, same halving discipline.
  (void) on_task;
  size_t size = want;
  while (size >= 4096) {
    raw = static_cast<uint8_t *>(malloc(size));  // NOLINT(cppcoreguidelines-no-malloc)
    if (raw != nullptr)
      break;
    size /= 2;
  }
#endif
  *actual_size = raw != nullptr ? size : 0;
  if (raw == nullptr)
    return RamBuffer(nullptr, RamBufferDeleter{0});
  return RamBuffer(raw, RamBufferDeleter{size});
}

// The chunk buffer the blocking (main-loop) copy paths borrow -- never on the worker task, so
// on_task is false: `want` bytes in internal RAM.
static RamBuffer alloc_copy_chunk(size_t want, size_t *chunk_size_out) {
  return alloc_dma_capable(want, /*on_task=*/false, chunk_size_out);
}

StorageError copy(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path) {
  // A directory is copied whole -- the API owns that, not its callers. Only pay for the stat()
  // when it decides something: a limit is configured, or we need to know what this path is.
  FileStat src_stat{};
  StorageError stat_err = src_storage->stat(src_path, &src_stat);
  if (stat_err != StorageError::OK)
    return stat_err;

  if (src_storage == dst_storage) {
    // Copying a file onto itself would truncate the destination before the first read of the
    // source, leaving an empty file where the data was. And copying a directory into its own
    // subtree walks what the walk is creating: it terminates on STORAGE_MAX_RECURSION_DEPTH,
    // but only after landing several levels of duplicates that the caller then has to undo.
    // Both are rejected outright; neither has a sensible outcome to produce.
    size_t src_len = strlen(src_path);
    if (strcmp(src_path, dst_path) == 0)
      return StorageError::INVALID_ARGS;
    if (src_stat.is_dir && strncmp(src_path, dst_path, src_len) == 0 && dst_path[src_len] == '/')
      return StorageError::INVALID_ARGS;
  }

  // A single file smaller than the chunk does not need the full chunk. The allocator floors at
  // 4 kB, so ask for at least that much rather than fall through its "even 4 kB failed" path.
  size_t want = STORAGE_COPY_CHUNK_SIZE;
  if (!src_stat.is_dir && src_stat.size < static_cast<uint64_t>(want)) {
    size_t needed = static_cast<size_t>(src_stat.size);
    want = needed > 4096 ? needed : 4096;
  }
  size_t chunk_size = 0;
  RamBuffer chunk_buf = alloc_copy_chunk(want, &chunk_size);
  if (chunk_buf == nullptr)
    return StorageError::NO_SPACE;

  if (src_stat.is_dir) {
    // The walk's two path buffers live here, once, and every level extends and truncates them.
    char src_buf[STORAGE_PATH_MAX];
    char dst_buf[STORAGE_PATH_MAX];
    int src_len = snprintf(src_buf, sizeof(src_buf), "%s", src_path);
    int dst_len = snprintf(dst_buf, sizeof(dst_buf), "%s", dst_path);
    if (src_len < 0 || static_cast<size_t>(src_len) >= sizeof(src_buf) || dst_len < 0 ||
        static_cast<size_t>(dst_len) >= sizeof(dst_buf))
      return StorageError::INVALID_ARGS;
    return copy_tree_at_depth(src_storage, src_buf, static_cast<size_t>(src_len), dst_storage, dst_buf,
                              static_cast<size_t>(dst_len), chunk_buf, chunk_size, 0);
  }

  StorageError err = check_blocking_transfer_size(src_stat.size);
  if (err != StorageError::OK)
    return err;
  return copy_one_file(src_storage, src_path, dst_storage, dst_path, chunk_buf, chunk_size);
}

StorageError move(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path) {
  if (src_storage == dst_storage) {
    StorageError err = src_storage->rename(src_path, dst_path);  // same-storage: O(1), no size limit
    // Only a refusal is worth redoing the long way; every other error would hit copy() too.
    if (err != StorageError::NOT_SUPPORTED || global_storage_registry == nullptr ||
        !global_storage_registry->get_move_fallback_copy())
      return err;
    ESP_LOGD(TAG, "rename refused for '%s' -- moving it as copy + remove instead", src_path);
  }

  // What the source is decides how it goes away afterwards; copy() already handles either.
  FileStat src_stat{};
  StorageError stat_err = src_storage->stat(src_path, &src_stat);
  if (stat_err != StorageError::OK)
    return stat_err;

  StorageError err = copy(src_storage, src_path, dst_storage, dst_path);
  if (err != StorageError::OK)
    return err;
  return src_stat.is_dir ? remove_recursive(src_storage, src_path) : src_storage->remove(src_path);
}

static StorageError remove_recursive_at_depth(PathStorage *storage, char *path, size_t len, size_t depth);

struct RemoveRecursiveCtx {
  PathStorage *storage;
  // Buffer owned by remove_recursive(), shared by every level; `len` is where this level's
  // directory path ends and a child segment gets appended.
  char *path;
  size_t len;
  size_t depth;
  StorageError err{StorageError::OK};
};

static bool remove_recursive_cb(const FileStat *entry, void *ctx_ptr) {
  auto *ctx = static_cast<RemoveRecursiveCtx *>(ctx_ptr);

  size_t len = append_path_segment(ctx->path, ctx->len, entry->name);
  if (len == 0) {
    ctx->err = StorageError::INVALID_ARGS;
    return false;  // path too long for the fixed buffer
  }

  StorageError err;
  if (entry->is_dir) {
    err = remove_recursive_at_depth(ctx->storage, ctx->path, len, ctx->depth + 1);
  } else {
    err = ctx->storage->remove(ctx->path);
  }

  // Back to this level's directory before the next entry is appended.
  ctx->path[ctx->len] = '\0';

  if (err != StorageError::OK) {
    ctx->err = err;
    return false;  // stop enumeration -- an entry failed
  }
  return true;
}

static StorageError remove_recursive_at_depth(PathStorage *storage, char *path, size_t len, size_t depth) {
  if (depth > STORAGE_MAX_RECURSION_DEPTH)
    return StorageError::INVALID_ARGS;

  RemoveRecursiveCtx ctx{storage, path, len, depth};
  StorageError err = storage->list_dir(path, remove_recursive_cb, &ctx);
  if (err != StorageError::OK)
    return err;
  if (ctx.err != StorageError::OK)
    return ctx.err;

  return storage->rmdir(path);
}

StorageError remove_recursive(PathStorage *storage, const char *path) {
  // The walk's path buffer lives here, once, and every level extends and truncates it.
  char buf[STORAGE_PATH_MAX];
  int len = snprintf(buf, sizeof(buf), "%s", path);
  if (len < 0 || static_cast<size_t>(len) >= sizeof(buf))
    return StorageError::INVALID_ARGS;
  return remove_recursive_at_depth(storage, buf, static_cast<size_t>(len), 0);
}

#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
// Log sinks for fatfs_select.h, which is header-only (it includes FatFs headers that only
// exist in the driver's build) and therefore cannot hold the log macros itself.
void fatfs_log_probe_read_failed(const char *tag) { ESP_LOGW(tag, "file_system probe: cannot read sector 0"); }

void fatfs_log_reformat_no_filesystem(const char *tag, bool want_exfat) {
  ESP_LOGW(tag, "file_system: no recognizable filesystem on the medium - formatting as %s",
           want_exfat ? "exFAT" : "FAT32");
}

void fatfs_log_reformat_mismatch(const char *tag, bool found_exfat, bool want_exfat) {
  ESP_LOGW(tag, "file_system: found %s but %s is configured - REFORMATTING, all data on the medium is erased",
           found_exfat ? "exFAT" : "FAT", want_exfat ? "exFAT" : "FAT32");
}

void fatfs_log_format_failed(const char *tag, bool want_exfat, int result) {
  ESP_LOGE(tag, "file_system: formatting as %s failed (FatFs error %d)", want_exfat ? "exFAT" : "FAT32", result);
}

void fatfs_log_format_done(const char *tag, bool want_exfat) {
  ESP_LOGI(tag, "file_system: medium formatted as %s", want_exfat ? "exFAT" : "FAT32");
}
#endif  // USE_STORAGE_FILE_SYSTEM_SELECT

}  // namespace esphome::storage
