#pragma once
#include "esphome/core/defines.h"

#ifdef USE_BINARY_STORAGE_LITTLEFS
#include "binary_storage.h"
#include "esphome/components/storage/storage.h"
#include <memory>

namespace esphome::binary_storage {

// Maximum simultaneously open files (kept low for MCU memory constraints)
static constexpr int LFS_VFS_MAX_FDS = 8;

// Forward-declared — defined in littlefs_mount.cpp alongside the VFS callbacks that use it
struct LfsVfsContext;

// Mounts a BinaryStorage device as a LittleFS filesystem in the ESP-IDF VFS.
// Extends FilesystemStorage — all file operations go through POSIX/VFS after mount.
class LittleFSMount : public storage::FilesystemStorage {
 public:
  LittleFSMount() = default;
  ~LittleFSMount();

  // Component lifecycle
  void setup() override;
  void loop() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 100.0f; }

  //========================================================================
  // Configuration setters (called by Python codegen)
  //========================================================================

  void set_storage_device(BinaryStorage *storage) { this->storage_ = storage; }
  void set_mount_path(const char *path) { this->set_mount_path_(path); }
  void set_auto_format(bool format) { this->auto_format_ = format; }

  //========================================================================
  // FilesystemStorage interface
  //========================================================================

  storage::StorageError get_info(storage::StorageInfo *info) override;
  // Task-safety is inherited from the backing device: lfs itself only needs the
  // per-instance serialization the caller already guarantees, but every block
  // callback lands on the underlying BinaryStorage — safe off the main loop only
  // if that device's I/O is. Bus-attached devices (I2C/SPI/OneWire) report 0, so
  // today this always resolves to 0; it lifts automatically for any future
  // task-safe backing without touching this class.
  uint8_t get_capabilities() const override {
    return this->storage_ != nullptr
               ? (this->storage_->get_capabilities() & storage::StorageCaps::STORAGE_CAP_IO_TASK_SAFE)
               : 0;
  }
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
  void list_files() const;

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  BinaryStorage *storage_{nullptr};
  bool auto_format_{true};
  bool mounted_{false};

  // Pool of FileHandles for open() — no heap allocation per open call
  storage::FileHandle handle_pool_[LFS_VFS_MAX_FDS]{};
  // Path storage for handle_pool_ entries (mount_path_ + "/" + filename)
  char handle_paths_[LFS_VFS_MAX_FDS][STORAGE_MAX_PATH_LEN]{};

  //========================================================================
  // LittleFS objects (opaque — lfs_t/lfs_config are managed component types,
  // only available when building for IDF with managed components downloaded)
  //========================================================================

  void *lfs_{nullptr};
  void *lfs_cfg_{nullptr};
  std::unique_ptr<uint8_t[]> read_buffer_;
  std::unique_ptr<uint8_t[]> prog_buffer_;
  std::unique_ptr<uint8_t[]> lookahead_buffer_;
  void *lfs_context_{nullptr};
  LfsVfsContext *vfs_context_{nullptr};

  //========================================================================
  // Internal helpers
  //========================================================================

  bool init_lfs_config_();
  bool mount_lfs_();
  bool unmount_lfs_();
  bool format_lfs_();
  void register_with_vfs_();

  storage::FileHandle *alloc_handle_(const char *path);
  void free_handle_(storage::FileHandle *handle);
};

}  // namespace esphome::binary_storage

#endif  // USE_BINARY_STORAGE_LITTLEFS
