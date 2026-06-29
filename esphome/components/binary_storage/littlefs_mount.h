#pragma once
#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#include "binary_storage.h"
#include "esphome/components/storage/storage.h"
#include "lfs.h"
#include <memory>
#include <dirent.h>

namespace esphome {
namespace binary_storage {

// Maximum simultaneously open files (kept low for MCU memory constraints)
static constexpr int LFS_VFS_MAX_FDS = 8;

class LittleFSMount;

// Holds filesystem state and the VFS file descriptor table
struct LfsVfsContext {
  lfs_t *lfs;
  lfs_config *cfg;
  LittleFSMount *mount;
  lfs_file_t files[LFS_VFS_MAX_FDS];
  bool fd_used[LFS_VFS_MAX_FDS];
  char *fd_paths[LFS_VFS_MAX_FDS];
};

// Directory handle wrapper for VFS readdir support (vfs_dir must be first)
struct LfsVfsDir {
  DIR vfs_dir;
  lfs_dir_t lfs_dir;
  struct dirent dirent;
  char *path;
};

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
  void set_mount_path(const char *path) { this->mount_path_ = path; }
  void set_auto_format(bool format) { this->auto_format_ = format; }

  //========================================================================
  // FilesystemStorage interface
  //========================================================================

  storage::StorageError get_info(storage::StorageInfo *info) override;
  storage::StorageError mount() override;
  storage::StorageError unmount() override;
  storage::StorageError format() override;
  storage::StorageError sync() override;
  storage::StorageError open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) override;
  storage::StorageError close(storage::FileHandle *handle) override;
  storage::StorageError read(storage::FileHandle *handle, uint8_t *buf, size_t len,
                             size_t *bytes_transferred) override;
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

  //========================================================================
  // Extras
  //========================================================================

  bool is_mounted() const { return this->mounted_; }
  const char *get_mount_path() const { return this->mount_path_; }
  bool remount();
  void list_files() const;

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  BinaryStorage *storage_{nullptr};
  const char *mount_path_{"/storage"};
  bool auto_format_{true};
  bool mounted_{false};

  // Pool of FileHandles for open() — no heap allocation per open call
  storage::FileHandle handle_pool_[LFS_VFS_MAX_FDS]{};
  // Path storage for handle_pool_ entries (mount_path_ + "/" + filename)
  char handle_paths_[LFS_VFS_MAX_FDS][STORAGE_MAX_PATH_LEN]{};

  //========================================================================
  // LittleFS objects
  //========================================================================

  std::unique_ptr<lfs_t> lfs_;
  std::unique_ptr<lfs_config> lfs_cfg_;
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

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_ESP_IDF
