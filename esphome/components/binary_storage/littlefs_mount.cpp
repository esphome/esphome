#ifdef USE_BINARY_STORAGE_LITTLEFS

#include "littlefs_mount.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "lfs.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>

namespace esphome::binary_storage {


static inline lfs_t *lfs_cast(void *p) { return static_cast<lfs_t *>(p); }
static inline lfs_config *cfg_cast(void *p) { return static_cast<lfs_config *>(p); }

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

static const char *const TAG = "littlefs_mount";

//========================================================================
// LittleFS Block Device Callbacks
//========================================================================

struct LittleFSContext {
  BinaryStorage *storage;
  BlockDeviceConfig config;
};

static int lfs_block_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer,
                                 lfs_size_t size) {
  auto *ctx = static_cast<LittleFSContext *>(c->context);
  return ctx->storage->block_read(block, off, buffer, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs_block_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer,
                                 lfs_size_t size) {
  auto *ctx = static_cast<LittleFSContext *>(c->context);
  return ctx->storage->block_prog(block, off, buffer, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs_block_device_erase(const struct lfs_config *c, lfs_block_t block) {
  auto *ctx = static_cast<LittleFSContext *>(c->context);
  return ctx->storage->block_erase(block) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int lfs_block_device_sync(const struct lfs_config *c) {
  auto *ctx = static_cast<LittleFSContext *>(c->context);
  return ctx->storage->block_sync() == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

//========================================================================
// VFS Wrapper Functions
//========================================================================

static int lfs_errno_remap(int lfs_err) {
  switch (lfs_err) {
    case LFS_ERR_OK:
      return 0;
    case LFS_ERR_IO:
    case LFS_ERR_CORRUPT:
      return EIO;
    case LFS_ERR_NOENT:
      return ENOENT;
    case LFS_ERR_EXIST:
      return EEXIST;
    case LFS_ERR_NOTDIR:
      return ENOTDIR;
    case LFS_ERR_ISDIR:
      return EISDIR;
    case LFS_ERR_NOTEMPTY:
      return ENOTEMPTY;
    case LFS_ERR_BADF:
      return EBADF;
    case LFS_ERR_FBIG:
      return EFBIG;
    case LFS_ERR_INVAL:
      return EINVAL;
    case LFS_ERR_NOSPC:
      return ENOSPC;
    case LFS_ERR_NOMEM:
      return ENOMEM;
    case LFS_ERR_NOATTR:
      return ENODATA;
    case LFS_ERR_NAMETOOLONG:
      return ENAMETOOLONG;
    default:
      return EIO;
  }
}

static int posix_flags_to_lfs(int flags) {
  // O_RDONLY=0, O_WRONLY=1, O_RDWR=2 map directly to LFS equivalents
  static const int ACC_TO_LFS[3] = {LFS_O_RDONLY, LFS_O_WRONLY, LFS_O_RDWR};
  const int acc = flags & O_ACCMODE;
  int lfs_flags = ACC_TO_LFS[acc < 3 ? acc : 0];

  if (flags & O_CREAT)
    lfs_flags |= LFS_O_CREAT;
  if (flags & O_EXCL)
    lfs_flags |= LFS_O_EXCL;
  if (flags & O_TRUNC)
    lfs_flags |= LFS_O_TRUNC;
  if (flags & O_APPEND)
    lfs_flags |= LFS_O_APPEND;

  return lfs_flags;
}

static int vfs_lfs_alloc_fd(LfsVfsContext *ctx) {
  for (int i = 0; i < LFS_VFS_MAX_FDS; i++) {
    if (!ctx->fd_used[i]) {
      ctx->fd_used[i] = true;
      ctx->fd_paths[i] = nullptr;
      return i;
    }
  }
  return -1;
}

static void vfs_lfs_free_fd(LfsVfsContext *ctx, int fd) {
  if (fd >= 0 && fd < LFS_VFS_MAX_FDS) {
    ctx->fd_used[fd] = false;
    if (ctx->fd_paths[fd]) {
      free(ctx->fd_paths[fd]);  // NOLINT(cppcoreguidelines-no-malloc)
      ctx->fd_paths[fd] = nullptr;
    }
  }
}

static int vfs_lfs_open(void *ctx, const char *path, int flags, int mode) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  int fd = vfs_lfs_alloc_fd(vfs_ctx);
  if (fd < 0) {
    errno = ENFILE;
    return -1;
  }

  int err = lfs_file_open(vfs_ctx->lfs, &vfs_ctx->files[fd], path, posix_flags_to_lfs(flags));
  if (err != LFS_ERR_OK) {
    vfs_lfs_free_fd(vfs_ctx, fd);
    errno = lfs_errno_remap(err);
    return -1;
  }

  vfs_ctx->fd_paths[fd] = strdup(path);  // NOLINT(cppcoreguidelines-no-malloc)
  ESP_LOGD(TAG, "VFS open: %s -> fd=%d", path, fd);
  return fd;
}

static int vfs_lfs_close(void *ctx, int fd) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  if (fd < 0 || fd >= LFS_VFS_MAX_FDS || !vfs_ctx->fd_used[fd]) {
    errno = EBADF;
    return -1;
  }

  int sync_err = lfs_file_sync(vfs_ctx->lfs, &vfs_ctx->files[fd]);
  if (sync_err != LFS_ERR_OK) {
    ESP_LOGW(TAG, "VFS close: sync failed for fd=%d, err=%d", fd, sync_err);
  }

  int err = lfs_file_close(vfs_ctx->lfs, &vfs_ctx->files[fd]);
  vfs_lfs_free_fd(vfs_ctx, fd);

  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }

  ESP_LOGD(TAG, "VFS close: fd=%d", fd);
  return 0;
}

static ssize_t vfs_lfs_read(void *ctx, int fd, void *dst, size_t size) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  if (fd < 0 || fd >= LFS_VFS_MAX_FDS || !vfs_ctx->fd_used[fd]) {
    errno = EBADF;
    return -1;
  }

  lfs_ssize_t result = lfs_file_read(vfs_ctx->lfs, &vfs_ctx->files[fd], dst, size);
  if (result < 0) {
    errno = lfs_errno_remap(result);
    return -1;
  }

  return result;
}

static ssize_t vfs_lfs_write(void *ctx, int fd, const void *data, size_t size) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  if (fd < 0 || fd >= LFS_VFS_MAX_FDS || !vfs_ctx->fd_used[fd]) {
    errno = EBADF;
    return -1;
  }

  lfs_ssize_t result = lfs_file_write(vfs_ctx->lfs, &vfs_ctx->files[fd], data, size);
  if (result < 0) {
    errno = lfs_errno_remap(result);
    return -1;
  }

  return result;
}

static off_t vfs_lfs_lseek(void *ctx, int fd, off_t offset, int whence) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  if (fd < 0 || fd >= LFS_VFS_MAX_FDS || !vfs_ctx->fd_used[fd]) {
    errno = EBADF;
    return -1;
  }

  static const int LFS_WHENCE_MAP[3] = {LFS_SEEK_SET, LFS_SEEK_CUR, LFS_SEEK_END};
  if (whence < 0 || whence > 2) {
    errno = EINVAL;
    return -1;
  }
  int lfs_whence = LFS_WHENCE_MAP[whence];

  lfs_soff_t result = lfs_file_seek(vfs_ctx->lfs, &vfs_ctx->files[fd], offset, lfs_whence);
  if (result < 0) {
    errno = lfs_errno_remap(result);
    return -1;
  }

  return result;
}

static int vfs_lfs_fstat(void *ctx, int fd, struct stat *st) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  if (fd < 0 || fd >= LFS_VFS_MAX_FDS || !vfs_ctx->fd_used[fd]) {
    errno = EBADF;
    return -1;
  }

  memset(st, 0, sizeof(struct stat));

  lfs_soff_t size = lfs_file_size(vfs_ctx->lfs, &vfs_ctx->files[fd]);
  if (size < 0) {
    errno = lfs_errno_remap(size);
    return -1;
  }

  st->st_size = size;
  st->st_mode = S_IFREG | 0644;
  st->st_blksize = vfs_ctx->cfg->block_size;
  st->st_blocks = (size + st->st_blksize - 1) / st->st_blksize;

  return 0;
}

static int vfs_lfs_stat(void *ctx, const char *path, struct stat *st) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  struct lfs_info info;
  int err = lfs_stat(vfs_ctx->lfs, path, &info);
  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }

  memset(st, 0, sizeof(struct stat));

  if (info.type == LFS_TYPE_DIR) {
    st->st_mode = S_IFDIR | 0755;
  } else {
    st->st_mode = S_IFREG | 0644;
    st->st_size = info.size;
  }

  st->st_blksize = vfs_ctx->cfg->block_size;
  st->st_blocks = (info.size + st->st_blksize - 1) / st->st_blksize;

  return 0;
}

static int vfs_lfs_unlink(void *ctx, const char *path) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  int err = lfs_remove(vfs_ctx->lfs, path);
  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }
  ESP_LOGD(TAG, "VFS unlink: %s", path);
  return 0;
}

static int vfs_lfs_rename(void *ctx, const char *src, const char *dst) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  int err = lfs_rename(vfs_ctx->lfs, src, dst);
  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }
  ESP_LOGD(TAG, "VFS rename: %s -> %s", src, dst);
  return 0;
}

static int vfs_lfs_mkdir(void *ctx, const char *name, mode_t mode) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  int err = lfs_mkdir(vfs_ctx->lfs, name);
  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }
  ESP_LOGD(TAG, "VFS mkdir: %s", name);
  return 0;
}

static int vfs_lfs_rmdir(void *ctx, const char *name) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  int err = lfs_remove(vfs_ctx->lfs, name);
  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }
  ESP_LOGD(TAG, "VFS rmdir: %s", name);
  return 0;
}

static DIR *vfs_lfs_opendir(void *ctx, const char *name) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  auto *dir = static_cast<LfsVfsDir *>(malloc(sizeof(LfsVfsDir)));  // NOLINT(cppcoreguidelines-no-malloc)
  if (!dir) {
    errno = ENOMEM;
    return nullptr;
  }

  memset(dir, 0, sizeof(LfsVfsDir));

  int err = lfs_dir_open(vfs_ctx->lfs, &dir->lfs_dir, name);
  if (err != LFS_ERR_OK) {
    free(dir);  // NOLINT(cppcoreguidelines-no-malloc)
    errno = lfs_errno_remap(err);
    return nullptr;
  }

  dir->path = strdup(name);  // NOLINT(cppcoreguidelines-no-malloc)
  ESP_LOGD(TAG, "VFS opendir: %s", name);

  return reinterpret_cast<DIR *>(dir);
}

static struct dirent *vfs_lfs_readdir(void *ctx, DIR *pdir) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  auto *dir = reinterpret_cast<LfsVfsDir *>(pdir);

  struct lfs_info info;

  while (true) {
    int err = lfs_dir_read(vfs_ctx->lfs, &dir->lfs_dir, &info);

    if (err == 0)
      return nullptr;  // End of directory
    if (err < 0) {
      errno = lfs_errno_remap(err);
      return nullptr;
    }

    if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
      continue;

    memset(&dir->dirent, 0, sizeof(struct dirent));
    strncpy(dir->dirent.d_name, info.name, sizeof(dir->dirent.d_name) - 1);
    dir->dirent.d_type = (info.type == LFS_TYPE_DIR) ? DT_DIR : DT_REG;

    return &dir->dirent;
  }
}

static long vfs_lfs_telldir(void *ctx, DIR *pdir) {  // NOLINT(google-runtime-int)
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  auto *dir = reinterpret_cast<LfsVfsDir *>(pdir);

  lfs_soff_t pos = lfs_dir_tell(vfs_ctx->lfs, &dir->lfs_dir);
  if (pos < 0) {
    errno = lfs_errno_remap(pos);
    return -1;
  }

  return static_cast<long>(pos);  // NOLINT(google-runtime-int)
}

static void vfs_lfs_seekdir(void *ctx, DIR *pdir, long loc) {  // NOLINT(google-runtime-int)
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  auto *dir = reinterpret_cast<LfsVfsDir *>(pdir);

  int err = lfs_dir_seek(vfs_ctx->lfs, &dir->lfs_dir, (lfs_off_t) loc);
  if (err < 0) {
    errno = lfs_errno_remap(err);
  }
}

static int vfs_lfs_closedir(void *ctx, DIR *pdir) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);
  auto *dir = reinterpret_cast<LfsVfsDir *>(pdir);

  int err = lfs_dir_close(vfs_ctx->lfs, &dir->lfs_dir);

  if (dir->path)
    free(dir->path);  // NOLINT(cppcoreguidelines-no-malloc)
  free(dir);          // NOLINT(cppcoreguidelines-no-malloc)

  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }

  return 0;
}

static int vfs_lfs_fsync(void *ctx, int fd) {
  auto *vfs_ctx = static_cast<LfsVfsContext *>(ctx);

  if (fd < 0 || fd >= LFS_VFS_MAX_FDS || !vfs_ctx->fd_used[fd]) {
    errno = EBADF;
    return -1;
  }

  int err = lfs_file_sync(vfs_ctx->lfs, &vfs_ctx->files[fd]);
  if (err != LFS_ERR_OK) {
    errno = lfs_errno_remap(err);
    return -1;
  }

  return 0;
}

//========================================================================
// LittleFSMount — lifecycle
//========================================================================

LittleFSMount::~LittleFSMount() {
  if (this->mounted_) {
    this->unmount_lfs_();
  }

  delete lfs_cast(this->lfs_);
  this->lfs_ = nullptr;
  delete cfg_cast(this->lfs_cfg_);
  this->lfs_cfg_ = nullptr;

  if (this->lfs_context_ != nullptr) {
    delete static_cast<LittleFSContext *>(this->lfs_context_);
    this->lfs_context_ = nullptr;
  }

  if (this->vfs_context_ != nullptr) {
    for (auto &fd_path : this->vfs_context_->fd_paths) {
      if (fd_path) {
        free(fd_path);  // NOLINT(cppcoreguidelines-no-malloc)
        fd_path = nullptr;
      }
    }
    delete this->vfs_context_;
    this->vfs_context_ = nullptr;
  }
}

void LittleFSMount::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LittleFS Mount...");

  if (this->storage_ == nullptr) {
    ESP_LOGE(TAG, "No storage device configured!");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "  Mount Path: %s", this->mount_path_);
  ESP_LOGCONFIG(TAG, "  Storage Device: %s", this->storage_->get_device_name());
  ESP_LOGCONFIG(TAG, "  Auto Format: %s", this->auto_format_ ? "YES" : "NO");

  if (!this->init_lfs_config_()) {
    ESP_LOGE(TAG, "Failed to initialize LittleFS configuration!");
    this->mark_failed();
    return;
  }

  if (this->mount() != storage::StorageError::OK) {
    ESP_LOGE(TAG, "Failed to mount LittleFS!");
    this->mark_failed();
  }
}

void LittleFSMount::dump_config() {
  ESP_LOGCONFIG(TAG, "LittleFS Mount:");
  ESP_LOGCONFIG(TAG, "  Mount Path: %s", this->mount_path_);
  ESP_LOGCONFIG(TAG, "  Device: %s (%s)", this->storage_->get_device_name(), this->storage_->get_device_type());
  ESP_LOGCONFIG(TAG, "  Mounted: %s", this->mounted_ ? "YES" : "NO");

  if (this->mounted_ && this->lfs_ != nullptr) {
    lfs_ssize_t block_count = lfs_fs_size(lfs_cast(this->lfs_));
    if (block_count >= 0) {
      uint32_t total_bytes = cfg_cast(this->lfs_cfg_)->block_count * cfg_cast(this->lfs_cfg_)->block_size;
      uint32_t used_bytes = block_count * cfg_cast(this->lfs_cfg_)->block_size;
      ESP_LOGCONFIG(TAG, "  Total: %" PRIu32 " bytes (%.1f KB)", total_bytes, total_bytes / 1024.0f);
      ESP_LOGCONFIG(TAG, "  Used:  %" PRIu32 " bytes (%.1f KB, %.1f%%)", used_bytes, used_bytes / 1024.0f,
                    (used_bytes * 100.0f) / total_bytes);
      ESP_LOGCONFIG(TAG, "  Free:  %" PRIu32 " bytes (%.1f KB)", total_bytes - used_bytes,
                    (total_bytes - used_bytes) / 1024.0f);
    }
  }
}

//========================================================================
// FilesystemStorage interface
//========================================================================

storage::StorageError LittleFSMount::get_info(storage::StorageInfo *info) {
  if (info == nullptr)
    return storage::StorageError::INVALID_ARGS;

  info->id = this->storage_ != nullptr ? this->storage_->get_device_type() : "littlefs";
  info->name = this->mount_path_;
  info->is_mounted = this->mounted_;
  info->is_removable = false;
  info->is_read_only = false;
  info->block_size = (this->lfs_cfg_ != nullptr) ? cfg_cast(this->lfs_cfg_)->block_size : 0;
  info->total_bytes = 0;
  info->free_bytes = 0;

  if (this->mounted_ && this->lfs_ != nullptr && this->lfs_cfg_ != nullptr) {
    lfs_ssize_t used_blocks = lfs_fs_size(lfs_cast(this->lfs_));
    if (used_blocks >= 0) {
      info->total_bytes =
          static_cast<uint64_t>(cfg_cast(this->lfs_cfg_)->block_count) * cfg_cast(this->lfs_cfg_)->block_size;
      uint64_t used_bytes = static_cast<uint64_t>(used_blocks) * cfg_cast(this->lfs_cfg_)->block_size;
      info->free_bytes = info->total_bytes > used_bytes ? info->total_bytes - used_bytes : 0;
    }
  }

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::mount() {
  if (!this->mount_lfs_())
    return storage::StorageError::READ_ERROR;

  this->register_with_vfs_();

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->register_storage(this);

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::unmount() {
  if (!this->unmount_lfs_())
    return storage::StorageError::WRITE_ERROR;

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->unregister_storage(this);

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::format() {
  return this->format_lfs_() ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError LittleFSMount::sync() {
  if (!this->mounted_ || this->vfs_context_ == nullptr)
    return storage::StorageError::NOT_READY;

  for (int i = 0; i < LFS_VFS_MAX_FDS; i++) {
    if (this->vfs_context_->fd_used[i]) {
      lfs_file_sync(lfs_cast(this->lfs_), &this->vfs_context_->files[i]);
    }
  }

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  // Build full VFS path (mount_path_ + path)
  char full_path[STORAGE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", this->mount_path_, path[0] == '/' ? path + 1 : path);

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

storage::StorageError LittleFSMount::close(storage::FileHandle *handle) {
  if (handle == nullptr || !handle->in_use)
    return storage::StorageError::INVALID_ARGS;
  if (handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;

  fclose(handle->file);
  handle->file = nullptr;
  this->free_handle_(handle);
  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::read(storage::FileHandle *handle, uint8_t *buf, size_t len,
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

storage::StorageError LittleFSMount::write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
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

storage::StorageError LittleFSMount::seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;

  int whence = SEEK_SET;
  if (mode == storage::SeekMode::CUR) {
    whence = SEEK_CUR;
  } else if (mode == storage::SeekMode::END) {
    whence = SEEK_END;
  }
  if (fseek(handle->file, static_cast<long>(offset), whence) != 0)
    return storage::StorageError::INVALID_ARGS;

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::tell(storage::FileHandle *handle, uint64_t *position) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr || position == nullptr)
    return storage::StorageError::INVALID_ARGS;

  long pos = ftell(handle->file);
  if (pos < 0)
    return storage::StorageError::READ_ERROR;

  *position = static_cast<uint64_t>(pos);
  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::stat(const char *path, storage::FileStat *stat_out) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr || stat_out == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", this->mount_path_, path[0] == '/' ? path + 1 : path);

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

storage::StorageError LittleFSMount::list_dir(const char *path,
                                              bool (*callback)(const storage::FileStat *entry, void *ctx), void *ctx) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr || callback == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", this->mount_path_, path[0] == '/' ? path + 1 : path);

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
      if (strlen(full_path) + 1 + strlen(entry->d_name) + 1 > STORAGE_MAX_PATH_LEN) {
        ESP_LOGE(TAG, "Path too long: %s/%s", full_path, entry->d_name);
        closedir(dir);
        return storage::StorageError::INVALID_ARGS;
      }
      snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, entry->d_name);
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

storage::StorageError LittleFSMount::mkdir(const char *path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", this->mount_path_, path[0] == '/' ? path + 1 : path);

  if (::mkdir(full_path, 0755) != 0)
    return errno == EEXIST ? storage::StorageError::INVALID_ARGS : storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::rmdir(const char *path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", this->mount_path_, path[0] == '/' ? path + 1 : path);

  // Non-recursive by contract — a populated directory must fail with NOT_EMPTY
  // (recursive delete is provided by the free storage::remove_recursive() helper).
  if (::rmdir(full_path) != 0)
    return errno == ENOTEMPTY ? storage::StorageError::NOT_EMPTY : storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::remove(const char *path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_path[STORAGE_MAX_PATH_LEN];
  snprintf(full_path, sizeof(full_path), "%s/%s", this->mount_path_, path[0] == '/' ? path + 1 : path);

  if (::unlink(full_path) != 0)
    return storage::StorageError::NOT_FOUND;

  return storage::StorageError::OK;
}

storage::StorageError LittleFSMount::rename(const char *old_path, const char *new_path) {
  if (!this->mounted_)
    return storage::StorageError::NOT_READY;
  if (old_path == nullptr || new_path == nullptr)
    return storage::StorageError::INVALID_ARGS;

  char full_old[STORAGE_MAX_PATH_LEN];
  char full_new[STORAGE_MAX_PATH_LEN];
  snprintf(full_old, sizeof(full_old), "%s/%s", this->mount_path_, old_path[0] == '/' ? old_path + 1 : old_path);
  snprintf(full_new, sizeof(full_new), "%s/%s", this->mount_path_, new_path[0] == '/' ? new_path + 1 : new_path);

  if (::rename(full_old, full_new) != 0)
    return storage::StorageError::WRITE_ERROR;

  return storage::StorageError::OK;
}

bool LittleFSMount::remount() {
  if (this->mounted_) {
    if (!this->unmount_lfs_())
      return false;
  }
  return this->mount_lfs_();
}

void LittleFSMount::list_files() const {
  if (!this->mounted_) {
    ESP_LOGW(TAG, "Filesystem not mounted, cannot list files");
    return;
  }

  ESP_LOGI(TAG, "Listing files in LittleFS at %s:", this->mount_path_);

  lfs_dir_t dir;
  int err = lfs_dir_open(lfs_cast(this->lfs_), &dir, "/");
  if (err != LFS_ERR_OK) {
    ESP_LOGE(TAG, "Failed to open root directory (err=%d)", err);
    return;
  }

  struct lfs_info info;
  while (true) {
    err = lfs_dir_read(lfs_cast(this->lfs_), &dir, &info);
    if (err <= 0)
      break;

    if (info.type == LFS_TYPE_REG) {
      ESP_LOGI(TAG, "  File: %s, Size: %" PRIu32, info.name, (uint32_t) info.size);
    } else if (info.type == LFS_TYPE_DIR) {
      ESP_LOGI(TAG, "  Directory: %s", info.name);
    }
  }

  lfs_dir_close(lfs_cast(this->lfs_), &dir);
}

//========================================================================
// Internal helpers
//========================================================================

bool LittleFSMount::init_lfs_config_() {
  BlockDeviceConfig block_config = this->storage_->get_block_config();

  ESP_LOGD(TAG,
           "Block device config: block_size=%" PRIu32 ", block_count=%" PRIu32 ", read_size=%" PRIu32
           ", prog_size=%" PRIu32,
           (uint32_t) block_config.block_size, (uint32_t) block_config.block_count, (uint32_t) block_config.read_size,
           (uint32_t) block_config.prog_size);

  this->lfs_ = new lfs_t();
  this->lfs_cfg_ = new lfs_config();

  auto *ctx = new LittleFSContext();
  ctx->storage = this->storage_;
  ctx->config = block_config;
  this->lfs_context_ = ctx;

  memset(cfg_cast(this->lfs_cfg_), 0, sizeof(lfs_config));

  cfg_cast(this->lfs_cfg_)->read = lfs_block_device_read;
  cfg_cast(this->lfs_cfg_)->prog = lfs_block_device_prog;
  cfg_cast(this->lfs_cfg_)->erase = lfs_block_device_erase;
  cfg_cast(this->lfs_cfg_)->sync = lfs_block_device_sync;

  cfg_cast(this->lfs_cfg_)->read_size = block_config.read_size;
  cfg_cast(this->lfs_cfg_)->prog_size = block_config.prog_size;
  cfg_cast(this->lfs_cfg_)->block_size = block_config.block_size;
  cfg_cast(this->lfs_cfg_)->block_count = block_config.block_count;
  cfg_cast(this->lfs_cfg_)->lookahead_size = block_config.lookahead_size;
  cfg_cast(this->lfs_cfg_)->cache_size = block_config.block_size;
  cfg_cast(this->lfs_cfg_)->block_cycles = 500;

  this->read_buffer_ = std::make_unique<uint8_t[]>(block_config.block_size);
  this->prog_buffer_ = std::make_unique<uint8_t[]>(block_config.block_size);
  this->lookahead_buffer_ = std::make_unique<uint8_t[]>(block_config.lookahead_size);

  cfg_cast(this->lfs_cfg_)->read_buffer = this->read_buffer_.get();
  cfg_cast(this->lfs_cfg_)->prog_buffer = this->prog_buffer_.get();
  cfg_cast(this->lfs_cfg_)->lookahead_buffer = this->lookahead_buffer_.get();
  cfg_cast(this->lfs_cfg_)->context = this->lfs_context_;

  return true;
}

bool LittleFSMount::mount_lfs_() {
  int err = lfs_mount(lfs_cast(this->lfs_), cfg_cast(this->lfs_cfg_));

  if (err != LFS_ERR_OK) {
    if (this->auto_format_) {
      ESP_LOGW(TAG, "Mount failed (err=%d), attempting to format...", err);

      err = lfs_format(lfs_cast(this->lfs_), cfg_cast(this->lfs_cfg_));
      if (err != LFS_ERR_OK) {
        ESP_LOGE(TAG, "Format failed (err=%d)", err);
        return false;
      }

      err = lfs_mount(lfs_cast(this->lfs_), cfg_cast(this->lfs_cfg_));
      if (err != LFS_ERR_OK) {
        ESP_LOGE(TAG, "Mount failed after format (err=%d)", err);
        return false;
      }
    } else {
      ESP_LOGE(TAG, "Mount failed (err=%d), auto_format disabled", err);
      return false;
    }
  }

  this->mounted_ = true;
  ESP_LOGI(TAG, "LittleFS mounted successfully at %s", this->mount_path_);
  return true;
}

bool LittleFSMount::unmount_lfs_() {
  if (!this->mounted_)
    return true;

  int err = lfs_unmount(lfs_cast(this->lfs_));
  if (err != LFS_ERR_OK) {
    ESP_LOGE(TAG, "Failed to unmount (err=%d)", err);
    return false;
  }

  this->mounted_ = false;
  return true;
}

bool LittleFSMount::format_lfs_() {
  ESP_LOGW(TAG, "Formatting LittleFS filesystem - ALL DATA WILL BE LOST!");

  bool was_mounted = this->mounted_;
  if (was_mounted)
    this->unmount_lfs_();

  int err = lfs_format(lfs_cast(this->lfs_), cfg_cast(this->lfs_cfg_));
  if (err != LFS_ERR_OK) {
    ESP_LOGE(TAG, "Format failed (err=%d)", err);
    return false;
  }

  ESP_LOGI(TAG, "Format successful");

  if (was_mounted)
    return this->mount_lfs_();

  return true;
}

void LittleFSMount::register_with_vfs_() {
  this->vfs_context_ = new LfsVfsContext();
  memset(this->vfs_context_, 0, sizeof(LfsVfsContext));
  this->vfs_context_->lfs = lfs_cast(this->lfs_);
  this->vfs_context_->cfg = cfg_cast(this->lfs_cfg_);
  this->vfs_context_->mount = this;

  for (int i = 0; i < LFS_VFS_MAX_FDS; i++) {
    this->vfs_context_->fd_used[i] = false;
    this->vfs_context_->fd_paths[i] = nullptr;
  }

  esp_vfs_t vfs = {};
  vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;

  vfs.write_p = &vfs_lfs_write;
  vfs.read_p = &vfs_lfs_read;
  vfs.open_p = &vfs_lfs_open;
  vfs.close_p = &vfs_lfs_close;
  vfs.fstat_p = &vfs_lfs_fstat;
  vfs.lseek_p = &vfs_lfs_lseek;
  vfs.fsync_p = &vfs_lfs_fsync;

  vfs.stat_p = &vfs_lfs_stat;
  vfs.unlink_p = &vfs_lfs_unlink;
  vfs.rename_p = &vfs_lfs_rename;
  vfs.opendir_p = &vfs_lfs_opendir;
  vfs.readdir_p = &vfs_lfs_readdir;
  vfs.telldir_p = &vfs_lfs_telldir;
  vfs.seekdir_p = &vfs_lfs_seekdir;
  vfs.closedir_p = &vfs_lfs_closedir;
  vfs.mkdir_p = &vfs_lfs_mkdir;
  vfs.rmdir_p = &vfs_lfs_rmdir;

  esp_err_t err = esp_vfs_register(this->mount_path_, &vfs, this->vfs_context_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register LittleFS with VFS: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "LittleFS registered with VFS at %s", this->mount_path_);
  }
}

storage::FileHandle *LittleFSMount::alloc_handle_(const char *path) {
  for (int i = 0; i < LFS_VFS_MAX_FDS; i++) {
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

void LittleFSMount::free_handle_(storage::FileHandle *handle) {
  if (handle == nullptr)
    return;
  handle->in_use = false;
  handle->path = nullptr;
  handle->storage = nullptr;
  handle->file = nullptr;
}

}  // namespace esphome::binary_storage

#endif  // USE_BINARY_STORAGE_LITTLEFS
