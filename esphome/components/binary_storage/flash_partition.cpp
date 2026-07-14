#include "flash_partition.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esp_littlefs.h"
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>

namespace esphome::binary_storage {

// STORAGE_MAX_PATH_LEN is defined in this component (see flash_partition.h / binary_storage.h).

static const char *const TAG = "flash_partition";

//========================================================================
// Lifecycle
//========================================================================

FlashPartition::~FlashPartition() {
  if (this->mounted_) {
    this->unmount_lfs_();
  }
}

void FlashPartition::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LittleFS partition '%s'...", this->partition_label_);

  esp_vfs_littlefs_conf_t conf = {
      .base_path = this->mount_path_,
      .partition_label = this->partition_label_,
      .format_if_mount_failed = this->auto_format_,
      .dont_mount = false,
  };

  esp_err_t ret = esp_vfs_littlefs_register(&conf);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find partition '%s'", this->partition_label_);
    } else {
      ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
    }
    this->mark_failed();
    return;
  }

  this->mounted_ = true;
  ESP_LOGI(TAG, "LittleFS mounted at '%s'", this->mount_path_);

  size_t total = 0, used = 0;
  if (esp_littlefs_info(this->partition_label_, &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "Partition size: total=%" PRIu32 ", used=%" PRIu32, (uint32_t) total, (uint32_t) used);
  }

  // Permanent registration: registered-but-unmounted is this device's normal state after a
  // manual unmount — mount()/unmount() only flip the mounted state (unmount quiesces), the
  // registry entry stays for the device's lifetime.
  if (storage::global_storage_registry != nullptr) {
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }
  }
}

void FlashPartition::dump_config() {
  ESP_LOGCONFIG(TAG, "LittleFS Flash Partition:");
  ESP_LOGCONFIG(TAG, "  Partition: %s", this->partition_label_);
  ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_);
  ESP_LOGCONFIG(TAG, "  Auto format: %s", YESNO(this->auto_format_));
  ESP_LOGCONFIG(TAG, "  Mounted: %s", YESNO(this->mounted_));

  if (this->mounted_) {
    size_t total = 0, used = 0;
    if (esp_littlefs_info(this->partition_label_, &total, &used) == ESP_OK) {
      ESP_LOGCONFIG(TAG, "  Total: %" PRIu32 " bytes", (uint32_t) total);
      ESP_LOGCONFIG(TAG, "  Used:  %" PRIu32 " bytes", (uint32_t) used);
      ESP_LOGCONFIG(TAG, "  Free:  %" PRIu32 " bytes", (uint32_t) (total - used));
    }
  }
}

//========================================================================
// FilesystemStorage interface
//========================================================================

storage::StorageError FlashPartition::get_info(storage::StorageInfo *info) {
  if (info == nullptr)
    return storage::StorageError::INVALID_ARGS;

  info->id = this->storage_id_ != nullptr ? this->storage_id_ : this->partition_label_;
  info->name = this->storage_name_ != nullptr ? this->storage_name_ : this->mount_path_;
  info->is_mounted = this->mounted_;
  info->is_removable = false;
  info->is_read_only = false;
  info->block_size = 4096;
  info->total_bytes = 0;
  info->free_bytes = 0;

  if (this->mounted_) {
    size_t total = 0, used = 0;
    if (esp_littlefs_info(this->partition_label_, &total, &used) == ESP_OK) {
      info->total_bytes = total;
      info->free_bytes = total > used ? total - used : 0;
    }
  }

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::mount() {
  if (this->mounted_)
    return storage::StorageError::OK;

  esp_vfs_littlefs_conf_t conf = {
      .base_path = this->mount_path_,
      .partition_label = this->partition_label_,
      .format_if_mount_failed = this->auto_format_,
      .dont_mount = false,
  };

  if (esp_vfs_littlefs_register(&conf) != ESP_OK)
    return storage::StorageError::READ_ERROR;

  this->mounted_ = true;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::unmount() {
  if (!this->mounted_)
    return storage::StorageError::OK;

  // Drain BEFORE teardown: after quiesce_storage() no worker data-plane call against this
  // device is in flight, so the esp_littlefs unregistration below (which frees the esp_vfs
  // slot) cannot race a running chunk. The registry entry stays (permanent registration,
  // see setup()).
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->quiesce_storage(this);

  if (!this->unmount_lfs_())
    return storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::format() {
  return this->format_lfs_() ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError FlashPartition::sync() {
  // esp_vfs_littlefs handles sync internally — no explicit flush needed
  return this->mounted_ ? storage::StorageError::OK : storage::StorageError::NOT_READY;
}

storage::StorageError FlashPartition::open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_path, sizeof(full_path), path);

  const char *fopen_mode;
  switch (mode) {
    case storage::OpenMode::READ:
      fopen_mode = "rb";
      break;
    case storage::OpenMode::WRITE:
      fopen_mode = "wb";
      break;
    case storage::OpenMode::APPEND:
      fopen_mode = "ab";
      break;
    case storage::OpenMode::READ_WRITE:
      fopen_mode = "r+b";
      break;
    default:
      fopen_mode = "rb";
      break;
  }

  FILE *f = fopen(full_path, fopen_mode);
  if (f == nullptr)
    return storage::StorageError::NOT_FOUND;

  storage::FileHandle *h = this->alloc_handle_(path);
  if (h == nullptr) {
    fclose(f);
    return storage::StorageError::NO_SPACE;
  }

  h->file = f;
  handle = h;
  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::close(storage::FileHandle *handle) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;

  fclose(handle->file);
  handle->file = nullptr;
  this->free_handle_(handle);
  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::read(storage::FileHandle *handle, uint8_t *buf, size_t len,
                                           size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr || buf == nullptr)
    return storage::StorageError::INVALID_ARGS;

  size_t n = fread(buf, 1, len, handle->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;

  if (n < len && ferror(handle->file))
    return storage::StorageError::READ_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                                            size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr || buf == nullptr)
    return storage::StorageError::INVALID_ARGS;

  size_t n = fwrite(buf, 1, len, handle->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;

  if (n < len)
    return storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;

  int whence = SEEK_SET;
  if (mode == storage::SeekMode::CUR) {
    whence = SEEK_CUR;
  } else if (mode == storage::SeekMode::END) {
    whence = SEEK_END;
  }
  if (fseek(handle->file, static_cast<int32_t>(offset), whence) != 0)
    return storage::StorageError::INVALID_ARGS;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::tell(storage::FileHandle *handle, uint64_t *position) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr || position == nullptr)
    return storage::StorageError::INVALID_ARGS;

  int32_t pos = ftell(handle->file);
  if (pos < 0)
    return storage::StorageError::READ_ERROR;

  *position = static_cast<uint64_t>(pos);
  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::stat(const char *path, storage::FileStat *stat_out) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr || stat_out == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_path, sizeof(full_path), path);

  struct stat st;
  if (::stat(full_path, &st) != 0)
    return storage::StorageError::NOT_FOUND;

  const char *base = strrchr(path, '/');
  base = (base != nullptr) ? base + 1 : path;  // FileStat.name is the basename only, by contract
  strncpy(stat_out->name, base, sizeof(stat_out->name) - 1);
  stat_out->name[sizeof(stat_out->name) - 1] = '\0';
  stat_out->size = (size_t) st.st_size;
  stat_out->is_dir = S_ISDIR(st.st_mode);

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::list_dir(const char *path,
                                               bool (*callback)(const storage::FileStat *entry, void *ctx), void *ctx) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr || callback == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_path, sizeof(full_path), path);

  DIR *dir = opendir(full_path);
  if (dir == nullptr)
    return storage::StorageError::NOT_FOUND;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    storage::FileStat fs_entry{};
    strncpy(fs_entry.name, entry->d_name, STORAGE_MAX_PATH_LEN - 1);
    fs_entry.name[STORAGE_MAX_PATH_LEN - 1] = '\0';
    fs_entry.is_dir = (entry->d_type == DT_DIR);
    fs_entry.size = 0;

    if (!fs_entry.is_dir) {
      char entry_path[STORAGE_MAX_PATH_LEN];
      const size_t base_len = strlen(full_path);
      const size_t name_len = strlen(entry->d_name);
      if (base_len + 1 + name_len + 1 > sizeof(entry_path)) {
        ESP_LOGE(TAG, "Path too long: %s/%s", full_path, entry->d_name);
        closedir(dir);
        return storage::StorageError::INVALID_ARGS;
      }
      // Exact-length join (the guard above makes it provably fitting) — snprintf here
      // draws -Wformat-truncation because GCC cannot see the runtime length check.
      memcpy(entry_path, full_path, base_len);
      entry_path[base_len] = '/';
      memcpy(entry_path + base_len + 1, entry->d_name, name_len + 1);
      struct stat st;
      if (::stat(entry_path, &st) == 0)
        fs_entry.size = (size_t) st.st_size;
    }

    if (!callback(&fs_entry, ctx))
      break;  // caller stopped enumeration early — not an error
  }

  closedir(dir);
  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::mkdir(const char *path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_path, sizeof(full_path), path);

  if (::mkdir(full_path, 0755) != 0)
    return errno == EEXIST ? storage::StorageError::INVALID_ARGS : storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::rmdir(const char *path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_path, sizeof(full_path), path);

  // Non-recursive by contract — a populated directory must fail with NOT_EMPTY
  // (recursive delete is provided by the free storage::remove_recursive() helper).
  if (::rmdir(full_path) != 0)
    return errno == ENOTEMPTY ? storage::StorageError::NOT_EMPTY : storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::remove(const char *path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_path, sizeof(full_path), path);

  if (::unlink(full_path) != 0)
    return storage::StorageError::NOT_FOUND;

  return storage::StorageError::OK;
}

storage::StorageError FlashPartition::rename(const char *old_path, const char *new_path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (old_path == nullptr || new_path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_old[STORAGE_MAX_PATH_LEN];
  char full_new[STORAGE_MAX_PATH_LEN];
  this->build_path_(full_old, sizeof(full_old), old_path);
  this->build_path_(full_new, sizeof(full_new), new_path);

  if (::rename(full_old, full_new) != 0)
    return storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

void FlashPartition::build_path_(char *out, size_t out_size, const char *path) const {
  // Join mount_path_ and a user-supplied path (with or without leading '/').
  // Exact-length memcpy join instead of snprintf — GCC cannot see callers'
  // length validation and would raise -Wformat-truncation (see list_dir()).
  const size_t base_len = strlen(this->mount_path_);
  if (base_len + 2 > out_size) {  // no room for even "<base>/" + NUL
    if (out_size > 0)
      out[0] = '\0';
    return;
  }
  const char *rel = (path != nullptr && path[0] == '/') ? path + 1 : (path != nullptr ? path : "");
  size_t rel_len = strlen(rel);
  const size_t max_rel = out_size - base_len - 2;
  if (rel_len > max_rel)
    rel_len = max_rel;  // defensive truncation; length-sensitive callers validate beforehand
  memcpy(out, this->mount_path_, base_len);
  out[base_len] = '/';
  memcpy(out + base_len + 1, rel, rel_len);
  out[base_len + 1 + rel_len] = '\0';
}

bool FlashPartition::remount() {
  // Used by format_lfs_() to bring the freshly formatted filesystem back up.
  // mount() is idempotent and carries the full register sequence.
  return this->mount() == storage::StorageError::OK;
}

bool FlashPartition::unmount_lfs_() {
  if (!this->mounted_)
    return true;

  esp_err_t ret = esp_vfs_littlefs_unregister(this->partition_label_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unmount: %s", esp_err_to_name(ret));
    return false;
  }

  this->mounted_ = false;
  ESP_LOGI(TAG, "Unmounted '%s'", this->mount_path_);
  return true;
}

bool FlashPartition::format_lfs_() {
  bool was_mounted = this->mounted_;
  if (was_mounted && !this->unmount_lfs_())
    return false;

  esp_err_t ret = esp_littlefs_format(this->partition_label_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to format: %s", esp_err_to_name(ret));
    return false;
  }

  ESP_LOGI(TAG, "Formatted partition '%s'", this->partition_label_);

  if (was_mounted)
    return this->remount();

  return true;
}

storage::FileHandle *FlashPartition::alloc_handle_(const char *path) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!this->handle_pool_[i].in_use) {
      this->handle_pool_[i].in_use = true;
      this->handle_pool_[i].storage = this;
      this->handle_pool_[i].file = nullptr;
      strncpy(this->handle_paths_[i], path != nullptr ? path : "", STORAGE_MAX_PATH_LEN - 1);
      this->handle_paths_[i][STORAGE_MAX_PATH_LEN - 1] = '\0';
      this->handle_pool_[i].path = this->handle_paths_[i];
      return &this->handle_pool_[i];
    }
  }
  return nullptr;
}

void FlashPartition::free_handle_(storage::FileHandle *handle) {
  if (handle == nullptr)
    return;
  handle->in_use = false;
  handle->path = nullptr;
  handle->storage = nullptr;
  handle->file = nullptr;
}

}  // namespace esphome::binary_storage

#endif  // USE_ESP_IDF
