#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/storage/storage.h"
#include <cstdint>

#ifdef USE_ESP_IDF
#include <esp_vfs.h>

namespace esphome::sd_storage {

enum class CardType : uint8_t {
  UNKNOWN = 0,
  SDIO = 1,
  MMC = 2,
  SDHC = 3,
  SDXC = 3,
  SDSC = 4,
};

struct SdFileHandle : public storage::FileHandle {
  char path_buf[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)]{};
};

// Base class for both SDMMC and SPI implementations.
// Extends FilesystemStorage so both SdMmc and SdSpi satisfy the storage interface.
class SdStorageBase : public storage::FilesystemStorage {
 public:
  void set_mount_path(const char *path) { this->mount_path_ = path; }
  void set_id(const char *id) { this->storage_id_ = id; }
  bool is_mounted() const { return this->is_mounted_; }
  const char *get_mount_path() const { return this->mount_path_; }

  template<typename F> void add_on_mounted_callback(F &&cb) { this->on_mounted_.add(std::forward<F>(cb)); }

  // Storage base interface
  storage::StorageError get_info(storage::StorageInfo *info) override;

  // FilesystemStorage virtuals — common POSIX implementations (subclasses provide mount/unmount)
  storage::StorageError format() override;
  storage::StorageError sync() override;
  storage::StorageError open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) override;
  storage::StorageError close(storage::FileHandle *handle) override;
  storage::StorageError read(storage::FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred) override;
  storage::StorageError write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                              size_t *bytes_transferred) override;
  storage::StorageError seek(storage::FileHandle *handle, size_t offset) override;
  storage::StorageError tell(storage::FileHandle *handle, size_t *position) override;
  storage::StorageError stat(const char *path, storage::FileStat *stat) override;
  storage::StorageError list_dir(const char *path, void (*callback)(const storage::FileStat *entry, void *ctx),
                                 void *ctx) override;
  storage::StorageError mkdir(const char *path) override;
  storage::StorageError rmdir(const char *path, bool recursive) override;
  storage::StorageError remove(const char *path) override;
  storage::StorageError rename(const char *old_path, const char *new_path) override;
  storage::StorageError copy(const char *src_path, const char *dst_path) override;

 protected:
  static constexpr int MAX_OPEN_FILES = 4;

  // Subclasses provide handle pool and card capacity/space info
  virtual SdFileHandle *get_handle_pool() = 0;
  virtual uint64_t get_free_bytes_impl() const = 0;
  virtual uint32_t get_block_size_impl() const = 0;

  // Build absolute VFS path from a relative path into caller-supplied buffer.
  // Returns false if the result would exceed buf_size.
  bool build_full_path_(const char *rel_path, char *buf, size_t buf_size) const;

  void log_mount_result_(bool success) const;
  void log_unmount_() const;
  void log_list_dir_start_(const char *path) const;
  static void log_list_dir_entry(const storage::FileStat *entry);

  CardType card_type_{CardType::UNKNOWN};
  bool is_mounted_{false};
  uint64_t total_bytes_{0};
  uint64_t used_bytes_{0};
  const char *mount_path_{"/sdcard"};
  const char *storage_id_{nullptr};

  LazyCallbackManager<void(const char *)> on_mounted_;
};

}  // namespace esphome::sd_storage

#endif  // USE_ESP_IDF
