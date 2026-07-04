#include "storage/storage.h"
#include "esphome/core/log.h"

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
    if (type == StorageType::FILESYSTEM)
      cb(static_cast<FilesystemStorage *>(s), ctx);
    else if (type == StorageType::NETWORK)
      cb(static_cast<NetworkStorage *>(s), ctx);
  }
}

//========================================================================
// Free helper functions
//========================================================================

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
  }

  out = std::move(buf);
  *size = total_read;
  return StorageError::OK;
}

StorageError write_file(FilesystemStorage *storage, const char *path, const uint8_t *data, size_t size) {
  FileHandle *handle = nullptr;
  StorageError err = storage->open(path, handle, OpenMode::WRITE);
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
  }
  return storage->close(handle);
}

StorageError write_file(NetworkStorage *storage, const char *path, const uint8_t *data, size_t size) {
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
  }
  return StorageError::OK;
}

StorageError copy(PathStorage *src_storage, const char *src_path, PathStorage *dst_storage, const char *dst_path) {
  RamBuffer buf;
  size_t size = 0;

  StorageError err;
  if (src_storage->get_storage_type() == StorageType::FILESYSTEM) {
    err = read_file(static_cast<FilesystemStorage *>(src_storage), src_path, buf, &size);
  } else {
    err = read_file(static_cast<NetworkStorage *>(src_storage), src_path, buf, &size);
  }
  if (err != StorageError::OK)
    return err;

  if (dst_storage->get_storage_type() == StorageType::FILESYSTEM) {
    return write_file(static_cast<FilesystemStorage *>(dst_storage), dst_path, buf.get(), size);
  }
  return write_file(static_cast<NetworkStorage *>(dst_storage), dst_path, buf.get(), size);
}

namespace {

struct RemoveRecursiveCtx {
  PathStorage *storage;
  const char *base_path;
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
    err = remove_recursive(ctx->storage, child_path);
  } else {
    err = ctx->storage->remove(child_path);
  }

  if (err != StorageError::OK) {
    ctx->err = err;
    return false;  // stop enumeration — an entry failed
  }
  return true;
}

}  // namespace

StorageError remove_recursive(PathStorage *storage, const char *path) {
  RemoveRecursiveCtx ctx{storage, path};
  StorageError err = storage->list_dir(path, remove_recursive_cb, &ctx);
  if (err != StorageError::OK)
    return err;
  if (ctx.err != StorageError::OK)
    return ctx.err;

  return storage->rmdir(path);
}

}  // namespace esphome::storage
