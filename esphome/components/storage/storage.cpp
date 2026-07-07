#include "storage.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::storage {

static const char *const TAG = "storage";

StorageRegistry *global_storage_registry = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void StorageRegistry::register_storage(Storage *s) {
  if (s == nullptr)
    return;

  for (auto *existing : this->storages_) {
    if (existing == s)
      return;  // already registered
  }

  if (this->storages_.full()) {
    ESP_LOGE(TAG, "Registry full — increase device count");
    return;
  }
  this->storages_.push_back(s);

  // Log regardless of get_info()'s result — an unmounted-but-registered device (e.g. before
  // its medium is mounted) must still show up, per the get_info() contract on Storage above.
  StorageInfo info{};
  if (s->get_info(&info) == StorageError::OK) {
    ESP_LOGI(TAG, "Storage registered: %s (%s)%s", info.name != nullptr ? info.name : "?",
             info.id != nullptr ? info.id : "?", info.is_mounted ? "" : " [not mounted]");
  } else {
    ESP_LOGI(TAG, "Storage registered (get_info failed)");
  }

  this->on_registered_.call(s);
}

void StorageRegistry::unregister_storage(Storage *s) {
  if (s == nullptr)
    return;

  // Swap-remove: no reallocation, keeps capacity — safe since order doesn't matter here
  // (for_each* enumerate every entry regardless of position).
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

  StorageInfo info{};
  if (s->get_info(&info) == StorageError::OK) {
    ESP_LOGI(TAG, "Storage unregistered: %s", info.name != nullptr ? info.name : "?");
  }

  this->on_unregistered_.call(s);
}

bool StorageRegistry::is_registered(const Storage *s) const {
  for (auto *registered : this->storages_) {
    if (registered == s) {
      return true;
    }
  }
  return false;
}

void StorageRegistry::for_each(void (*cb)(Storage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    cb(s, ctx);
  }
}

void StorageRegistry::for_each_filesystem(void (*cb)(FilesystemStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    if (s->get_storage_type() == StorageType::FILESYSTEM)
      cb(static_cast<FilesystemStorage *>(s), ctx);
  }
}

void StorageRegistry::for_each_raw(void (*cb)(RawStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    if (s->get_storage_type() == StorageType::RAW)
      cb(static_cast<RawStorage *>(s), ctx);
  }
}

void StorageRegistry::for_each_network(void (*cb)(NetworkStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    if (s->get_storage_type() == StorageType::NETWORK)
      cb(static_cast<NetworkStorage *>(s), ctx);
  }
}

void StorageRegistry::for_each_path_based(void (*cb)(PathStorage *s, void *ctx), void *ctx) {
  for (auto *s : this->storages_) {
    StorageType type = s->get_storage_type();
    if (type == StorageType::FILESYSTEM) {
      cb(static_cast<FilesystemStorage *>(s), ctx);
    } else if (type == StorageType::NETWORK) {
      cb(static_cast<NetworkStorage *>(s), ctx);
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
    const char *mount = ps->get_mount_path();
    if (mount == nullptr)
      continue;
    StringRef mount_ref(mount);
    size_t mount_len = mount_ref.size();

    // Prefix match only at a '/' boundary or an exact match — "/sd2/x" must not match "/sd".
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
  StringRef rel_ref(rel);
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

namespace {

// Shared guard-rail check for the blocking helpers below. No-op (returns OK) when no limit is
// configured (registry unset, or max_blocking_transfer_size == 0).
StorageError check_blocking_transfer_size(uint64_t size) {
  if (global_storage_registry == nullptr)
    return StorageError::OK;
  uint64_t limit = global_storage_registry->get_max_blocking_transfer_size();
  if (limit != 0 && size > limit)
    return StorageError::TRANSFER_TOO_LARGE;
  return StorageError::OK;
}

}  // namespace

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
  }
  return "UNKNOWN";
}

bool exists(PathStorage *storage, const char *path) {
  FileStat stat{};
  return storage->stat(path, &stat) == StorageError::OK;
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
  // fit in a size_t before it gets silently truncated by the allocation below.
  if (stat.size > SIZE_MAX)
    return StorageError::NO_SPACE;
  auto buf_size = static_cast<size_t>(stat.size);

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
      break;  // EOF before stat()-reported size — return what we got
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
  // fit in a size_t before it gets silently truncated by the allocation below.
  if (stat.size > SIZE_MAX)
    return StorageError::NO_SPACE;
  auto buf_size = static_cast<size_t>(stat.size);

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
      break;  // EOF before stat()-reported size — return what we got
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

StorageError copy(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path) {
  // Only pay for the extra stat() when a limit is actually configured — the guard-rail is
  // opt-in and copy() itself doesn't need the file size to stream.
  if (global_storage_registry != nullptr && global_storage_registry->get_max_blocking_transfer_size() != 0) {
    FileStat src_stat{};
    StorageError stat_err = src_storage->stat(src_path, &src_stat);
    if (stat_err != StorageError::OK)
      return stat_err;
    stat_err = check_blocking_transfer_size(src_stat.size);
    if (stat_err != StorageError::OK)
      return stat_err;
  }

  // PREFER_INTERNAL: PSRAM isn't DMA-capable on classic ESP32 (restricted on S3 too), so
  // SD/SPI drivers would bounce-buffer anyway. Fall back to smaller sizes under memory pressure.
  size_t chunk_size = STORAGE_COPY_CHUNK_SIZE;
  uint8_t *raw = nullptr;
  while (chunk_size >= 4096) {
    raw = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL).allocate(chunk_size);
    if (raw != nullptr)
      break;
    chunk_size /= 2;
  }
  if (raw == nullptr)
    return StorageError::NO_SPACE;
  RamBuffer chunk_buf(raw, RamBufferDeleter{chunk_size});

  bool src_is_fs = src_storage->get_storage_type() == StorageType::FILESYSTEM;
  bool dst_is_fs = dst_storage->get_storage_type() == StorageType::FILESYSTEM;
  auto *src_fs = src_is_fs ? static_cast<FilesystemStorage *>(src_storage) : nullptr;
  auto *src_net = src_is_fs ? nullptr : static_cast<NetworkStorage *>(src_storage);
  auto *dst_fs = dst_is_fs ? static_cast<FilesystemStorage *>(dst_storage) : nullptr;
  auto *dst_net = dst_is_fs ? nullptr : static_cast<NetworkStorage *>(dst_storage);

  FileHandle *src_handle = nullptr;
  FileHandle *dst_handle = nullptr;
  StorageError err;

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
    src_fs->close(src_handle);  // close result intentionally ignored — source is read-only
  if (dst_is_fs) {
    // FATFS flushes on close; a failed close on the destination means a silently truncated/
    // corrupt file, so its result must win over an OK from the copy loop above.
    StorageError close_err = dst_fs->close(dst_handle);
    if (err == StorageError::OK)
      err = close_err;
  }
  return err;
}

StorageError move(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path) {
  if (src_storage == dst_storage)
    return src_storage->rename(src_path, dst_path);  // same-storage: O(1), no size limit

  StorageError err = copy(src_storage, src_path, dst_storage, dst_path);
  if (err != StorageError::OK)
    return err;
  return src_storage->remove(src_path);
}

namespace {

StorageError remove_recursive_at_depth(PathStorage *storage, const char *path, size_t depth);

struct RemoveRecursiveCtx {
  PathStorage *storage;
  const char *base_path;
  size_t depth;
  StorageError err{StorageError::OK};
};

bool remove_recursive_cb(const FileStat *entry, void *ctx_ptr) {
  auto *ctx = static_cast<RemoveRecursiveCtx *>(ctx_ptr);

  // Fixed stack buffer instead of std::string — no heap allocation per entry during recursion.
  char child_path[STORAGE_NAME_MAX * 2 + 2];
  int written = snprintf(child_path, sizeof(child_path), "%s/%s", ctx->base_path, entry->name);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(child_path)) {
    ctx->err = StorageError::INVALID_ARGS;
    return false;  // path too long for the fixed buffer
  }

  StorageError err;
  if (entry->is_dir) {
    err = remove_recursive_at_depth(ctx->storage, child_path, ctx->depth + 1);
  } else {
    err = ctx->storage->remove(child_path);
  }

  if (err != StorageError::OK) {
    ctx->err = err;
    return false;  // stop enumeration — an entry failed
  }
  return true;
}

StorageError remove_recursive_at_depth(PathStorage *storage, const char *path, size_t depth) {
  if (depth > STORAGE_MAX_RECURSION_DEPTH)
    return StorageError::INVALID_ARGS;

  RemoveRecursiveCtx ctx{storage, path, depth};
  StorageError err = storage->list_dir(path, remove_recursive_cb, &ctx);
  if (err != StorageError::OK)
    return err;
  if (ctx.err != StorageError::OK)
    return ctx.err;

  return storage->rmdir(path);
}

}  // namespace

StorageError remove_recursive(PathStorage *storage, const char *path) {
  return remove_recursive_at_depth(storage, path, 0);
}

}  // namespace esphome::storage
