#include "sd_storage_base.h"

#include "esphome/core/log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include "esphome/components/storage/storage.h"

namespace esphome::sd_storage {

using storage::FileHandle;
using storage::FileStat;
using storage::OpenMode;
using storage::StorageError;
using storage::StorageInfo;

static const char *const TAG_BASE = "sd_storage";

bool SdStorageBase::build_full_path_(const char *rel_path, char *buf, size_t buf_size) const {
  size_t mount_len = strlen(this->mount_path_);
  bool needs_sep = (rel_path[0] != '/');
  size_t total = mount_len + (needs_sep ? 1 : 0) + strlen(rel_path) + 1;
  if (total > buf_size)
    return false;
  memcpy(buf, this->mount_path_, mount_len);
  size_t pos = mount_len;
  if (needs_sep)
    buf[pos++] = '/';
  strlcpy(buf + pos, rel_path, buf_size - pos);
  return true;
}

storage::StorageError SdStorageBase::get_info(storage::StorageInfo *info) {
  info->id = this->mount_path_;
  info->name = "SD Card";
  info->total_bytes = this->total_bytes_;
  info->free_bytes = this->get_free_bytes_impl();
  info->block_size = this->get_block_size_impl();
  info->is_mounted = this->is_mounted_;
  info->is_removable = true;
  info->is_read_only = false;
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::format() {
  ESP_LOGW(TAG_BASE, "Format not implemented for SD cards");
  return storage::StorageError::NOT_SUPPORTED;
}

storage::StorageError SdStorageBase::sync() { return storage::StorageError::OK; }

storage::StorageError SdStorageBase::open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  SdFileHandle *pool = this->get_handle_pool();
  SdFileHandle *h = nullptr;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!pool[i].in_use) {
      h = &pool[i];
      break;
    }
  }
  if (h == nullptr)
    return storage::StorageError::TOO_MANY_OPEN_FILES;

  if (!this->build_full_path_(path, h->path_buf, sizeof(h->path_buf)))
    return storage::StorageError::INVALID_ARGS;

  const char *fmode = nullptr;
  switch (mode) {
    case storage::OpenMode::READ:
      fmode = "rb";
      break;
    case storage::OpenMode::WRITE:
      fmode = "wb";
      break;
    case storage::OpenMode::APPEND:
      fmode = "ab";
      break;
    case storage::OpenMode::READ_WRITE:
      fmode = "r+b";
      break;
  }

  FILE *f = fopen(h->path_buf, fmode);
  if (f == nullptr)
    return storage::StorageError::NOT_FOUND;

  h->in_use = true;
  h->path = h->path_buf;
  h->storage = this;
  h->file = f;
  handle = h;
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::close(storage::FileHandle *handle) {
  if (handle == nullptr || !handle->in_use)
    return storage::StorageError::INVALID_ARGS;
  if (handle->file != nullptr) {
    fclose(handle->file);
    handle->file = nullptr;
  }
  handle->in_use = false;
  handle->path = nullptr;
  handle->storage = nullptr;
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::read(storage::FileHandle *handle, uint8_t *buf, size_t len,
                                          size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  size_t n = fread(buf, 1, len, handle->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                                           size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  size_t n = fwrite(buf, 1, len, handle->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  return (n == len) ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError SdStorageBase::seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  int whence;
  switch (mode) {
    case storage::SeekMode::SET:
      whence = SEEK_SET;
      break;
    case storage::SeekMode::CUR:
      whence = SEEK_CUR;
      break;
    case storage::SeekMode::END:
      whence = SEEK_END;
      break;
    default:
      return storage::StorageError::INVALID_ARGS;
  }
  return fseek(handle->file, static_cast<long>(offset), whence) == 0 ? storage::StorageError::OK
                                                                      : storage::StorageError::READ_ERROR;
}

storage::StorageError SdStorageBase::tell(storage::FileHandle *handle, uint64_t *position) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  long pos = ftell(handle->file);
  if (pos < 0)
    return storage::StorageError::READ_ERROR;
  *position = static_cast<uint64_t>(pos);
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::stat(const char *path, storage::FileStat *st) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_full_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  struct stat s;
  if (::stat(full, &s) != 0)
    return storage::StorageError::NOT_FOUND;

  strncpy(st->name, path, sizeof(st->name) - 1);
  st->name[sizeof(st->name) - 1] = '\0';
  st->size = S_ISDIR(s.st_mode) ? 0 : static_cast<uint64_t>(s.st_size);
  st->is_dir = S_ISDIR(s.st_mode);
  st->mtime = static_cast<uint32_t>(s.st_mtime);
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::list_dir(const char *path,
                                              bool (*callback)(const storage::FileStat *entry, void *ctx),
                                              void *ctx) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_full_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  DIR *dir = opendir(full);
  if (dir == nullptr)
    return storage::StorageError::NOT_FOUND;

  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;

    storage::FileStat entry{};
    strncpy(entry.name, ent->d_name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.is_dir = (ent->d_type == DT_DIR);

    char rel[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
    snprintf(rel, sizeof(rel), "%s/%s", path, ent->d_name);
    char entry_full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
    if (this->build_full_path_(rel, entry_full, sizeof(entry_full))) {
      struct stat s;
      if (::stat(entry_full, &s) == 0) {
        entry.size = entry.is_dir ? 0 : static_cast<uint64_t>(s.st_size);
        entry.mtime = static_cast<uint32_t>(s.st_mtime);
      }
    }

    // callback returns false to stop enumeration early — that is not an error, so we still
    // return OK below regardless of how the loop exits.
    if (!callback(&entry, ctx))
      break;
  }

  closedir(dir);
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::mkdir(const char *path) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_full_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  if (::mkdir(full, 0755) == 0)
    return storage::StorageError::OK;
  return (errno == EEXIST) ? storage::StorageError::ALREADY_EXISTS : storage::StorageError::WRITE_ERROR;
}

storage::StorageError SdStorageBase::rmdir(const char *path) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_full_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  // Non-recursive per the storage:: contract: must fail with NOT_EMPTY if the directory has
  // contents. Recursive delete is the free storage::remove_recursive() helper, built on top
  // of list_dir()/remove()/this rmdir() — no need to duplicate that tree-walk here.
  DIR *dir = opendir(full);
  if (dir == nullptr)
    return storage::StorageError::NOT_FOUND;

  bool has_entries = false;
  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;
    has_entries = true;
    break;
  }
  closedir(dir);
  if (has_entries)
    return storage::StorageError::NOT_EMPTY;

  return ::unlink(full) == 0 ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError SdStorageBase::remove(const char *path) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_full_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  return ::remove(full) == 0 ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError SdStorageBase::rename(const char *old_path, const char *new_path) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full_old[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  char full_new[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_full_path_(old_path, full_old, sizeof(full_old)))
    return storage::StorageError::INVALID_ARGS;
  if (!this->build_full_path_(new_path, full_new, sizeof(full_new)))
    return storage::StorageError::INVALID_ARGS;

  return ::rename(full_old, full_new) == 0 ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

void SdStorageBase::log_mount_result_(bool success) const {
  if (success) {
    ESP_LOGI(TAG_BASE, "Card mounted via automation");
  } else {
    ESP_LOGE(TAG_BASE, "Failed to mount card via automation");
  }
}

void SdStorageBase::log_unmount_() const { ESP_LOGI(TAG_BASE, "Card unmounted via automation"); }

void SdStorageBase::log_list_dir_start_(const char *path) const { ESP_LOGI(TAG_BASE, "Listing files in: %s", path); }

bool SdStorageBase::log_list_dir_entry(const storage::FileStat *entry, void *ctx) {
  if (entry->is_dir) {
    ESP_LOGI(TAG_BASE, "  [DIR]  %s", entry->name);
  } else {
    ESP_LOGI(TAG_BASE, "  [FILE] %s (%llu bytes)", entry->name, static_cast<unsigned long long>(entry->size));
  }
  return true;  // keep enumerating — this is a "list everything" action
}

}  // namespace esphome::sd_storage
