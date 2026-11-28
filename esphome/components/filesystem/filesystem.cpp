#include "filesystem.h"

#include <utility>

#include "esphome/core/progmem.h"

namespace esphome::filesystem {

ESPHOME_LOG_TAG(TAG, "filesystem");

// Indexed by the negated FsError value; the last entry is the fallback
PROGMEM_STRING_TABLE(FsErrorStrings, "none", "I/O error", "no media", "not found", "already exists", "not a directory",
                     "is a directory", "no space", "too many open", "invalid argument", "no filesystem", "read only",
                     "access denied", "closed", "busy", "unknown");

const LogString *fs_error_to_string(FsError error) { return FsErrorStrings::get_log_str(-static_cast<int>(error)); }

// File

File &File::operator=(File &&other) noexcept {
  if (this != &other) {
    this->close();
    this->take_(other);
  }
  return *this;
}

void File::take_(File &other) {
  this->fs_ = std::exchange(other.fs_, nullptr);
  this->slot_ = other.slot_;
  this->mount_ = other.mount_;
}

int32_t File::read(void *buf, uint32_t len) {
  if (this->fs_ == nullptr)
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  // The mount is checked under the lock, as another task may unmount
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  return this->fs_->fs_read(this->slot_, buf, len);
}

int32_t File::write(const void *buf, uint32_t len) {
  if (this->fs_ == nullptr)
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  return this->fs_->fs_write(this->slot_, buf, len);
}

int32_t File::seek(int32_t offset, Whence whence) {
  if (this->fs_ == nullptr)
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  return this->fs_->fs_seek(this->slot_, offset, whence);
}

int32_t File::tell() {
  if (this->fs_ == nullptr)
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  return this->fs_->fs_tell(this->slot_);
}

int32_t File::size() {
  if (this->fs_ == nullptr)
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return static_cast<int32_t>(FsError::FS_ERROR_CLOSED);
  return this->fs_->fs_size(this->slot_);
}

FsError File::sync() {
  if (this->fs_ == nullptr)
    return FsError::FS_ERROR_CLOSED;
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return FsError::FS_ERROR_CLOSED;
  return this->fs_->fs_sync(this->slot_);
}

FsError File::truncate() {
  if (this->fs_ == nullptr)
    return FsError::FS_ERROR_CLOSED;
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return FsError::FS_ERROR_CLOSED;
  return this->fs_->fs_truncate(this->slot_);
}

bool File::is_open() const { return this->fs_ != nullptr && this->fs_->file_valid_(this->mount_); }

FsError File::close() {
  Filesystem *fs = std::exchange(this->fs_, nullptr);
  if (fs == nullptr)
    return FsError::FS_ERROR_NONE;
  return fs->release_file_(this->slot_, this->mount_);
}

// Dir

Dir &Dir::operator=(Dir &&other) noexcept {
  if (this != &other) {
    this->close();
    this->take_(other);
  }
  return *this;
}

void Dir::take_(Dir &other) {
  this->fs_ = std::exchange(other.fs_, nullptr);
  this->slot_ = other.slot_;
  this->mount_ = other.mount_;
}

FsError Dir::read_next(FileInfo &info, char *name, size_t name_size) {
  if (this->fs_ == nullptr)
    return FsError::FS_ERROR_CLOSED;
  const std::scoped_lock lock{this->fs_->mutex_};
  if (!this->fs_->file_valid_(this->mount_))
    return FsError::FS_ERROR_CLOSED;
  if (name == nullptr || name_size == 0)
    return FsError::FS_ERROR_INVALID;
  return this->fs_->fs_read_dir(this->slot_, info, name, name_size);
}

bool Dir::is_open() const { return this->fs_ != nullptr && this->fs_->file_valid_(this->mount_); }

FsError Dir::close() {
  Filesystem *fs = std::exchange(this->fs_, nullptr);
  if (fs == nullptr)
    return FsError::FS_ERROR_NONE;
  return fs->release_dir_(this->slot_, this->mount_);
}

// Filesystem

void Filesystem::setup() {
  FsError err = this->fs_init(this->max_files_, this->max_dirs_);
  if (err != FsError::FS_ERROR_NONE) {
    this->mark_failed(fs_error_to_string(err));
    return;
  }
  if (this->device_->has_media()) {
    const std::scoped_lock lock{this->mutex_};
    this->ready_();
  }
}

void Filesystem::on_shutdown() {
  // Other tasks' file calls then wait until the restart instead of writing to the media while it is closed. The
  // main loop holds the lock, so later calls from it still work.
  this->mutex_.lock();  // never unlocked
  FsError err = this->unmount();
  if (err != FsError::FS_ERROR_NONE) {
    ESP_LOGW(TAG, "Closing files failed: %s", LOG_STR_ARG(fs_error_to_string(err)));
  }
}

void Filesystem::dump_config() {
  ESP_LOGCONFIG(TAG,
                "  Mounted: %s\n"
                "  Max open files: %u\n"
                "  Max open directories: %u\n"
                "  Format if mount fails: %s",
                YESNO(this->mounted_), this->max_files_, this->max_dirs_, YESNO(this->format_if_mount_failed_));
}

FsError Filesystem::mount() {
  const std::scoped_lock lock{this->mutex_};
  this->mount_error_ = FsError::FS_ERROR_NONE;
  return this->ready_();
}

FsError Filesystem::unmount() {
  const std::scoped_lock lock{this->mutex_};
  if (!this->mounted_)
    return FsError::FS_ERROR_NONE;
  FsError first = FsError::FS_ERROR_NONE;
  // Close what is still open so buffered data reaches the media while it is there. After a media change the
  // buffers belong to the old media; writing them would damage whatever is in the slot now.
  if (this->media_current_()) {
    for (uint8_t slot = 0; slot != this->max_files_; slot++) {
      if (this->files_in_use_ & (1UL << slot)) {
        FsError err = this->fs_close(slot);
        if (first == FsError::FS_ERROR_NONE)
          first = err;
      }
    }
    for (uint8_t slot = 0; slot != this->max_dirs_; slot++) {
      if (this->dirs_in_use_ & (1U << slot)) {
        FsError err = this->fs_close_dir(slot);
        if (first == FsError::FS_ERROR_NONE)
          first = err;
      }
    }
  }
  this->files_in_use_ = 0;
  this->dirs_in_use_ = 0;
  this->fs_unmount();
  this->mounted_ = false;
  ESP_LOGD(TAG, "Unmounted");
  return first;
}

FsError Filesystem::ready_() {
  // Before setup(), or after fs_init() failed, the driver has no state to mount with
  if (!this->is_ready())
    return FsError::FS_ERROR_INVALID;
  if (this->mounted_ && this->media_current_())
    return FsError::FS_ERROR_NONE;
  // A log callback that uses the filesystem while it mounts or formats
  if (this->changing_)
    return FsError::FS_ERROR_BUSY;
  this->changing_ = true;
  FsError err = this->mount_media_();
  this->changing_ = false;
  return err;
}

FsError Filesystem::mount_media_() {
  if (this->mounted_) {
    ESP_LOGI(TAG, "Media changed");
    this->unmount();
  }
  if (!this->device_->has_media())
    return FsError::FS_ERROR_NO_MEDIA;
  if (this->mount_error_ != FsError::FS_ERROR_NONE && this->media_current_())
    return this->mount_error_;
  this->media_generation_ = this->device_->get_media_generation();
  FsError err = this->fs_mount();
  if (err == FsError::FS_ERROR_NO_FILESYSTEM && this->format_if_mount_failed_) {
    ESP_LOGW(TAG, "No filesystem found, formatting");
    err = this->fs_format();
    if (err == FsError::FS_ERROR_NONE)
      err = this->fs_mount();
  }
  if (err != FsError::FS_ERROR_NONE) {
    this->mount_error_ = err;
    ESP_LOGW(TAG, "Mount failed: %s", LOG_STR_ARG(fs_error_to_string(err)));
    return err;
  }
  this->mount_error_ = FsError::FS_ERROR_NONE;
  this->mount_count_++;
  this->mounted_ = true;
  ESP_LOGD(TAG, "Mounted");
  return FsError::FS_ERROR_NONE;
}

File Filesystem::open(const char *path, uint8_t flags) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err != FsError::FS_ERROR_NONE) {
    this->fail_(err);
    return {};
  }
  if (path == nullptr || (flags & (OPEN_READ | OPEN_WRITE)) == 0) {
    this->fail_(FsError::FS_ERROR_INVALID);
    return {};
  }
  for (uint8_t slot = 0; slot != this->max_files_; slot++) {
    if (this->files_in_use_ & (1UL << slot))
      continue;
    err = this->fs_open(slot, path, flags);
    if (err != FsError::FS_ERROR_NONE) {
      this->fail_(err);
      return {};
    }
    this->files_in_use_ |= 1UL << slot;
    this->last_error_ = FsError::FS_ERROR_NONE;
    return {this, slot, this->mount_count_};
  }
  this->fail_(FsError::FS_ERROR_TOO_MANY_OPEN);
  return {};
}

Dir Filesystem::open_dir(const char *path) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err != FsError::FS_ERROR_NONE) {
    this->fail_(err);
    return {};
  }
  if (path == nullptr) {
    this->fail_(FsError::FS_ERROR_INVALID);
    return {};
  }
  for (uint8_t slot = 0; slot != this->max_dirs_; slot++) {
    if (this->dirs_in_use_ & (1U << slot))
      continue;
    err = this->fs_open_dir(slot, path);
    if (err != FsError::FS_ERROR_NONE) {
      this->fail_(err);
      return {};
    }
    this->dirs_in_use_ |= 1U << slot;
    this->last_error_ = FsError::FS_ERROR_NONE;
    return {this, slot, this->mount_count_};
  }
  this->fail_(FsError::FS_ERROR_TOO_MANY_OPEN);
  return {};
}

FsError Filesystem::release_file_(uint8_t slot, uint16_t mount) {
  const std::scoped_lock lock{this->mutex_};
  // A handle from an earlier mount has no driver state left; its slot was freed by unmount()
  if (!this->file_valid_(mount))
    return FsError::FS_ERROR_CLOSED;
  this->files_in_use_ &= ~(1UL << slot);
  return this->fs_close(slot);
}

FsError Filesystem::release_dir_(uint8_t slot, uint16_t mount) {
  const std::scoped_lock lock{this->mutex_};
  if (!this->file_valid_(mount))
    return FsError::FS_ERROR_CLOSED;
  this->dirs_in_use_ &= ~(1U << slot);
  return this->fs_close_dir(slot);
}

FsError Filesystem::remove(const char *path) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err == FsError::FS_ERROR_NONE)
    err = path == nullptr ? FsError::FS_ERROR_INVALID : this->fs_remove(path);
  return this->fail_(err);
}

FsError Filesystem::rename(const char *from, const char *to) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err == FsError::FS_ERROR_NONE)
    err = from == nullptr || to == nullptr ? FsError::FS_ERROR_INVALID : this->fs_rename(from, to);
  return this->fail_(err);
}

FsError Filesystem::stat(const char *path, FileInfo &info) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err == FsError::FS_ERROR_NONE)
    err = path == nullptr ? FsError::FS_ERROR_INVALID : this->fs_stat(path, info);
  return this->fail_(err);
}

FsError Filesystem::mkdir(const char *path) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err == FsError::FS_ERROR_NONE)
    err = path == nullptr ? FsError::FS_ERROR_INVALID : this->fs_mkdir(path);
  return this->fail_(err);
}

FsError Filesystem::space(uint64_t &total, uint64_t &free) {
  const std::scoped_lock lock{this->mutex_};
  FsError err = this->ready_();
  if (err == FsError::FS_ERROR_NONE)
    err = this->fs_space(total, free);
  return this->fail_(err);
}

FsError Filesystem::format() {
  const std::scoped_lock lock{this->mutex_};
  if (!this->is_ready())
    return this->fail_(FsError::FS_ERROR_INVALID);
  if (this->changing_)
    return this->fail_(FsError::FS_ERROR_BUSY);
  this->changing_ = true;
  this->unmount();
  FsError err = FsError::FS_ERROR_NO_MEDIA;
  if (this->device_->has_media()) {
    err = this->fs_format();
    if (err == FsError::FS_ERROR_NONE) {
      this->mount_error_ = FsError::FS_ERROR_NONE;
      err = this->mount_media_();
    }
  }
  this->changing_ = false;
  return this->fail_(err);
}

}  // namespace esphome::filesystem
