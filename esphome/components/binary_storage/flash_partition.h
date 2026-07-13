#pragma once
#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#include "esphome/components/storage/storage.h"
#include "esp_partition.h"
#include "binary_storage.h"  // shared STORAGE_MAX_PATH_LEN

namespace esphome::binary_storage {

// LittleFS on an internal flash partition via ESP-IDF's esp_vfs_littlefs.
// Simpler than LittleFSMount — IDF handles VFS registration and LittleFS internals.
// The partition must be defined in the partition table with subtype=littlefs.
class FlashPartition : public storage::FilesystemStorage {
 public:
  FlashPartition() = default;
  ~FlashPartition();

  // Component lifecycle
  void setup() override;
  void loop() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  //========================================================================
  // Configuration setters (called by Python codegen)
  //========================================================================

  void set_partition_label(const char *label) { this->partition_label_ = label; }
  void set_mount_path(const char *path) { this->set_mount_path_(path); }
  void set_auto_format(bool format) { this->auto_format_ = format; }
  void set_storage_id(const char *id) { this->storage_id_ = id; }
  void set_storage_name(const char *name) { this->storage_name_ = name; }

  //========================================================================
  // FilesystemStorage interface
  //========================================================================

  storage::StorageError get_info(storage::StorageInfo *info) override;
  // esp_littlefs guards every lfs operation with an internal mutex and esp_partition
  // read/write/erase are task-safe in ESP-IDF, so the data plane may run on the async
  // worker task (calls per instance are externally serialized by the caller anyway).
  uint8_t get_capabilities() const override { return storage::StorageCaps::STORAGE_CAP_IO_TASK_SAFE; }
  storage::StorageError mount() override;
  storage::StorageError unmount() override;
  storage::StorageError format() override;
  storage::StorageError sync() override;
  storage::StorageError open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) override;
  storage::StorageError close(storage::FileHandle *handle) override;
  storage::StorageError read(storage::FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred) override;
  storage::StorageError write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                              size_t *bytes_transferred) override;
  storage::StorageError seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) override;
  using storage::FilesystemStorage::seek;  // keep the (handle, offset) convenience overload visible
  storage::StorageError tell(storage::FileHandle *handle, uint64_t *position) override;
  storage::StorageError stat(const char *path, storage::FileStat *stat) override;
  storage::StorageError list_dir(const char *path, bool (*callback)(const storage::FileStat *entry, void *ctx),
                                 void *ctx) override;
  storage::StorageError mkdir(const char *path) override;
  storage::StorageError rmdir(const char *path) override;
  storage::StorageError remove(const char *path) override;
  storage::StorageError rename(const char *old_path, const char *new_path) override;

  //========================================================================
  // Extras
  //========================================================================

  bool is_mounted() const { return this->mounted_; }
  bool remount();

 protected:
  const char *partition_label_{nullptr};
  const char *storage_id_{nullptr};
  const char *storage_name_{nullptr};
  bool auto_format_{true};
  bool mounted_{false};

  // Pool of FileHandles for open() — no heap allocation per open call
  static constexpr int MAX_OPEN_FILES = 8;
  storage::FileHandle handle_pool_[MAX_OPEN_FILES]{};
  char handle_paths_[MAX_OPEN_FILES][STORAGE_MAX_PATH_LEN]{};

  //========================================================================
  // Internal helpers
  //========================================================================

  void build_path_(char *out, size_t out_size, const char *path) const;
  bool unmount_lfs_();
  bool format_lfs_();
  storage::FileHandle *alloc_handle_(const char *path);
  void free_handle_(storage::FileHandle *handle);
};

}  // namespace esphome::binary_storage

#endif  // USE_ESP_IDF
