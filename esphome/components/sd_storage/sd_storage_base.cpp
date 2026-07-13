#include "sd_storage_base.h"

#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
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

namespace {

// Maps a FATFS FRESULT to the closest StorageError. `for_rmdir` selects FR_DENIED's mapping:
// f_unlink() (used for rmdir — see rmdir() below, FATFS has no dedicated f_rmdir) returns
// FR_DENIED both for "directory not empty" and for genuine permission/read-only failures, so the
// caller must tell us which context applies.
StorageError fresult_to_storage_error(FRESULT res, bool for_rmdir, bool is_write) {
  switch (res) {
    case FR_OK:
      return StorageError::OK;
    case FR_NO_FILE:
    case FR_NO_PATH:
      return StorageError::NOT_FOUND;
    case FR_EXIST:
      return StorageError::ALREADY_EXISTS;
    case FR_DENIED:
      return for_rmdir ? StorageError::NOT_EMPTY : StorageError::PERMISSION_DENIED;
    case FR_INVALID_NAME:
      return StorageError::INVALID_ARGS;
    case FR_NOT_READY:
      return StorageError::NOT_READY;
    case FR_WRITE_PROTECTED:
      return StorageError::PERMISSION_DENIED;
    default:
      return is_write ? StorageError::WRITE_ERROR : StorageError::READ_ERROR;
  }
}

}  // namespace

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

bool SdStorageBase::build_fatfs_path_(const char *rel_path, char *buf, size_t buf_size) const {
  StringRef drive_ref(this->fatfs_drive_);
  StringRef rel_ref(rel_path);
  bool needs_sep = rel_ref.empty() || rel_ref[0] != '/';

  size_t total = drive_ref.size() + (needs_sep ? 1 : 0) + rel_ref.size() + 1;
  if (total > buf_size)
    return false;

  size_t pos = drive_ref.copy(buf, drive_ref.size());
  if (needs_sep)
    buf[pos++] = '/';
  pos += rel_ref.copy(buf + pos, rel_ref.size());
  buf[pos] = '\0';
  return true;
}

storage::StorageError SdStorageBase::get_info(storage::StorageInfo *info) {
  info->id = this->storage_id_ != nullptr ? this->storage_id_ : this->mount_path_;
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

void SdStorageBase::loop_cd_() {
  // No CD pin: card management is boot mount + manual actions only. In particular this
  // must NOT auto-remount after a manual unmount (card_present_() reports always-present
  // without a pin, so any level-based logic would remount on the next poll).
  if (this->cd_pin_ == nullptr)
    return;

  // Debounce tracking must run every loop() iteration (see card_present_()), the
  // mount/unmount reaction only at the poll interval.
  bool present = this->card_present_();
  if (!this->should_poll_cd_())
    return;

  if (!this->cd_state_seeded_) {
    // Seed with the current state — the boot-time mount decision was setup()'s.
    this->cd_last_present_ = present;
    this->cd_state_seeded_ = true;
    return;
  }
  if (present == this->cd_last_present_)
    return;  // level unchanged — edges only
  this->cd_last_present_ = present;

  if (present) {
    ESP_LOGI(TAG_BASE, "Card inserted (CD edge)");
    bool ok = this->mount() == storage::StorageError::OK;
    this->log_mount_result_(ok);
    if (ok)
      this->on_inserted_.call();
  } else if (this->is_mounted_) {
    ESP_LOGI(TAG_BASE, "Card removed (CD edge)");
    this->unmount();
    this->on_removed_.call();
  }
}

bool SdStorageBase::card_present_() {
  if (this->cd_pin_ == nullptr)
    return true;

  // Active-low: the switch pulls the pin low when a card is seated. digital_read() already
  // applies the pin's own inverted: flag, so this is the only place that convention is baked
  // in — hardware wired the other way is handled by setting inverted: true on cd_pin, not by
  // a second inversion setting here.
  bool raw_present = !this->cd_pin_->digital_read();
  uint32_t now = millis();

  if (!this->cd_debounce_started_) {
    // First call: seed both the confirmed and candidate state from the current reading so an
    // already-present card at boot doesn't look like a spurious insertion.
    this->cd_debounce_started_ = true;
    this->last_confirmed_present_ = raw_present;
    this->candidate_present_ = raw_present;
    this->candidate_since_ms_ = now;
    return this->last_confirmed_present_;
  }

  if (raw_present != this->candidate_present_) {
    // Raw reading changed — restart the debounce window from here.
    this->candidate_present_ = raw_present;
    this->candidate_since_ms_ = now;
  } else if (raw_present != this->last_confirmed_present_ && now - this->candidate_since_ms_ >= CD_DEBOUNCE_MS) {
    // Candidate has held steady for the full window — accept it.
    this->last_confirmed_present_ = raw_present;
  }

  return this->last_confirmed_present_;
}

bool SdStorageBase::should_poll_cd_() {
  uint32_t now = millis();
  if (!this->cd_poll_started_) {
    this->cd_poll_started_ = true;
    this->last_cd_poll_ms_ = now;
    return true;
  }
  if (now - this->last_cd_poll_ms_ < CD_POLL_MS)
    return false;
  this->last_cd_poll_ms_ = now;
  return true;
}

storage::StorageError SdStorageBase::sync() {
  SdFileHandle *pool = this->get_handle_pool();
  storage::StorageError err = storage::StorageError::OK;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!pool[i].in_use || pool[i].file == nullptr)
      continue;
    if (fflush(pool[i].file) != 0 || fsync(fileno(pool[i].file)) != 0)
      err = storage::StorageError::WRITE_ERROR;
  }
  return err;
}

storage::StorageError SdStorageBase::flush_open_handles_() {
  SdFileHandle *pool = this->get_handle_pool();
  storage::StorageError err = storage::StorageError::OK;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!pool[i].in_use)
      continue;
    if (pool[i].file != nullptr) {
      ESP_LOGW(TAG_BASE, "File still open at unmount, flushing and closing: %s", pool[i].path);
      if (fflush(pool[i].file) != 0 || fsync(fileno(pool[i].file)) != 0)
        err = storage::StorageError::WRITE_ERROR;
      if (fclose(pool[i].file) != 0)
        err = storage::StorageError::WRITE_ERROR;
      pool[i].file = nullptr;
    }
    pool[i].in_use = false;
    pool[i].path = nullptr;
    pool[i].storage = nullptr;
  }
  return err;
}

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
  if (f == nullptr) {
    switch (errno) {
      case ENOENT:
        return storage::StorageError::NOT_FOUND;
      case ENOSPC:
        return storage::StorageError::NO_SPACE;
      case EACCES:
      case EROFS:
        return storage::StorageError::PERMISSION_DENIED;
      case EMFILE:
      case ENFILE:
        return storage::StorageError::TOO_MANY_OPEN_FILES;
      default:
        return mode == storage::OpenMode::READ ? storage::StorageError::READ_ERROR : storage::StorageError::WRITE_ERROR;
    }
  }

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
  storage::StorageError err = storage::StorageError::OK;
  if (handle->file != nullptr) {
    // FATFS flushes on close (see the dst-close-error propagation in storage.cpp's copy()) —
    // a failed close here can mean a silently truncated/corrupt file.
    if (fclose(handle->file) != 0)
      err = storage::StorageError::WRITE_ERROR;
    handle->file = nullptr;
  }
  handle->in_use = false;
  handle->path = nullptr;
  handle->storage = nullptr;
  return err;
}

storage::StorageError SdStorageBase::read(storage::FileHandle *handle, uint8_t *buf, size_t len,
                                          size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  size_t n = fread(buf, 1, len, handle->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  // fread() returning less than requested means either EOF (not an error, per the
  // partial-read contract in storage.h) or a real I/O error — ferror() disambiguates.
  if (n < len && ferror(handle->file)) {
    clearerr(handle->file);
    return storage::StorageError::READ_ERROR;
  }
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
  // ESP-IDF's newlib fseek() takes a 32-bit `long` offset — FATFS/POSIX on this platform can't
  // address beyond that anyway (files >4GB aren't representable here), so reject rather than
  // silently truncating a caller-supplied 64-bit offset (the interface allows >4GB elsewhere,
  // e.g. NetworkStorage).
  if (offset > INT32_MAX || offset < INT32_MIN)
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
  return fseek(handle->file, static_cast<int32_t>(offset), whence) == 0 ? storage::StorageError::OK
                                                                        : storage::StorageError::READ_ERROR;
}

storage::StorageError SdStorageBase::tell(storage::FileHandle *handle, uint64_t *position) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  int32_t pos = ftell(handle->file);
  if (pos < 0)
    return storage::StorageError::READ_ERROR;
  *position = static_cast<uint64_t>(pos);
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::stat(const char *path, storage::FileStat *st) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  // f_stat() on the drive root ("N:/") fails by FATFS design — synthesize the result instead of
  // calling it. path is "" or "/" exactly when the caller asked to stat the mount point itself.
  if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
    st->name[0] = '\0';
    st->size = 0;
    st->is_dir = true;
    st->mtime = 0;
    return storage::StorageError::OK;
  }

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  FILINFO fno;
  FRESULT res = f_stat(full, &fno);
  if (res != FR_OK)
    return fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/false);

  // FileStat::name is the basename only (see the contract on the struct in storage.h) —
  // consistent with what list_dir() puts there, regardless of how many path segments the
  // caller passed in.
  StringRef path_ref(path);
  size_t base_pos = 0;
  for (size_t i = path_ref.size(); i > 0; i--) {
    if (path_ref[i - 1] == '/') {
      base_pos = i;
      break;
    }
  }
  StringRef base_ref(path + base_pos, path_ref.size() - base_pos);
  size_t name_len = base_ref.copy(st->name, sizeof(st->name) - 1);
  st->name[name_len] = '\0';
  st->is_dir = (fno.fattrib & AM_DIR) != 0;
  st->size = st->is_dir ? 0 : static_cast<uint64_t>(fno.fsize);
  st->mtime = 0;  // FatFs FILINFO exposes fdate/ftime (DOS format), not a Unix timestamp.
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::list_dir(const char *path,
                                              bool (*callback)(const storage::FileStat *entry, void *ctx), void *ctx) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  FF_DIR fat_dir;
  FRESULT res = f_opendir(&fat_dir, full);
  if (res != FR_OK)
    return fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/false);

  FILINFO fno;
  for (;;) {
    if (f_readdir(&fat_dir, &fno) != FR_OK || fno.fname[0] == '\0')
      break;
    StringRef fname_ref(fno.fname);
    if (fname_ref == "." || fname_ref == "..")
      continue;

    storage::FileStat entry{};
    size_t name_len = fname_ref.copy(entry.name, sizeof(entry.name) - 1);
    entry.name[name_len] = '\0';
    entry.is_dir = (fno.fattrib & AM_DIR) != 0;
    entry.size = entry.is_dir ? 0 : static_cast<uint64_t>(fno.fsize);
    entry.mtime = 0;  // FatFs FILINFO exposes fdate/ftime (DOS format), not a Unix timestamp.

    // callback returns false to stop enumeration early — that is not an error, so we still
    // return OK below regardless of how the loop exits.
    if (!callback(&entry, ctx))
      break;
  }

  f_closedir(&fat_dir);
  return storage::StorageError::OK;
}

storage::StorageError SdStorageBase::mkdir(const char *path) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  FRESULT res = f_mkdir(full);
  return fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/true);
}

storage::StorageError SdStorageBase::rmdir(const char *path) {
  if (!this->is_mounted_)
    return storage::StorageError::NOT_READY;

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  // Non-recursive per the storage:: contract: must fail with NOT_EMPTY if the directory has
  // contents. Recursive delete is the free storage::remove_recursive() helper, built on top
  // of list_dir()/remove()/this rmdir() — no need to duplicate that tree-walk here.
  FF_DIR fat_dir;
  FRESULT res = f_opendir(&fat_dir, full);
  if (res != FR_OK)
    return fresult_to_storage_error(res, /*for_rmdir=*/true, /*is_write=*/false);

  bool has_entries = false;
  FILINFO fno;
  while (f_readdir(&fat_dir, &fno) == FR_OK && fno.fname[0] != '\0') {
    StringRef fname_ref(fno.fname);
    if (fname_ref == "." || fname_ref == "..")
      continue;
    has_entries = true;
    break;
  }
  f_closedir(&fat_dir);
  if (has_entries)
    return storage::StorageError::NOT_EMPTY;

  // FATFS removes empty directories via f_unlink() — there is no dedicated f_rmdir().
  res = f_unlink(full);
  return fresult_to_storage_error(res, /*for_rmdir=*/true, /*is_write=*/true);
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

const char *SdStorageBase::card_type_to_string(CardType type) {
  switch (type) {
    case CardType::SDIO:
      return "SDIO";
    case CardType::MMC:
      return "MMC";
    case CardType::SDHC:
      return "SDHC/SDXC";
    case CardType::SDSC:
      return "SDSC";
    case CardType::UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

}  // namespace esphome::sd_storage
