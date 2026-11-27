#pragma once

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "esphome/components/filesystem/filesystem.h"

namespace esphome::filesystem::testing {

// A flat, in-memory driver: enough to exercise the handle pools, mounting and media changes in the base class.
class FakeFs : public Filesystem {
 public:
  using Filesystem::Filesystem;

  bool has_filesystem{true};
  int mounts{0};
  int formats{0};
  int closes{0};
  std::map<std::string, std::vector<uint8_t>> files;

 protected:
  struct Open {
    std::string path;
    uint32_t pos{0};
  };

  FsError fs_init(uint8_t max_files, uint8_t max_dirs) override {
    this->open_.resize(max_files);
    this->dir_pos_.resize(max_dirs);
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_mount() override {
    this->mounts++;
    return this->has_filesystem ? FsError::FS_ERROR_NONE : FsError::FS_ERROR_NO_FILESYSTEM;
  }
  void fs_unmount() override {}
  FsError fs_format() override {
    this->formats++;
    this->files.clear();
    this->has_filesystem = true;
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_open(uint8_t slot, const char *path, uint8_t flags) override {
    auto it = this->files.find(path);
    if (it == this->files.end()) {
      if (!(flags & OPEN_CREATE))
        return FsError::FS_ERROR_NOT_FOUND;
      it = this->files.emplace(path, std::vector<uint8_t>{}).first;
    } else if ((flags & OPEN_CREATE) && (flags & OPEN_EXCLUSIVE)) {
      return FsError::FS_ERROR_EXISTS;
    }
    if (flags & OPEN_TRUNCATE)
      it->second.clear();
    this->open_[slot] = {path, (flags & OPEN_APPEND) ? static_cast<uint32_t>(it->second.size()) : 0};
    return FsError::FS_ERROR_NONE;
  }
  int32_t fs_read(uint8_t slot, void *buf, uint32_t len) override {
    auto &f = this->open_[slot];
    auto &data = this->files[f.path];
    const uint32_t n = std::min<uint32_t>(len, data.size() - std::min<size_t>(f.pos, data.size()));
    memcpy(buf, data.data() + f.pos, n);
    f.pos += n;
    return static_cast<int32_t>(n);
  }
  int32_t fs_write(uint8_t slot, const void *buf, uint32_t len) override {
    auto &f = this->open_[slot];
    auto &data = this->files[f.path];
    if (data.size() < f.pos + len)
      data.resize(f.pos + len);
    memcpy(data.data() + f.pos, buf, len);
    f.pos += len;
    return static_cast<int32_t>(len);
  }
  int32_t fs_seek(uint8_t slot, int32_t offset, Whence whence) override {
    auto &f = this->open_[slot];
    int32_t base = 0;
    if (whence == Whence::WHENCE_CURRENT)
      base = static_cast<int32_t>(f.pos);
    if (whence == Whence::WHENCE_END)
      base = static_cast<int32_t>(this->files[f.path].size());
    f.pos = static_cast<uint32_t>(base + offset);
    return static_cast<int32_t>(f.pos);
  }
  int32_t fs_tell(uint8_t slot) override { return static_cast<int32_t>(this->open_[slot].pos); }
  int32_t fs_size(uint8_t slot) override { return static_cast<int32_t>(this->files[this->open_[slot].path].size()); }
  FsError fs_sync(uint8_t slot) override { return FsError::FS_ERROR_NONE; }
  FsError fs_truncate(uint8_t slot) override {
    this->files[this->open_[slot].path].resize(this->open_[slot].pos);
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_close(uint8_t slot) override {
    this->closes++;
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_open_dir(uint8_t slot, const char *path) override {
    this->dir_pos_[slot] = 0;
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_read_dir(uint8_t slot, FileInfo &info, char *name, size_t name_size) override {
    if (this->dir_pos_[slot] >= this->files.size())
      return FsError::FS_ERROR_NOT_FOUND;
    auto it = std::next(this->files.begin(), this->dir_pos_[slot]++);
    info.size = it->second.size();
    info.is_dir = false;
    strncpy(name, it->first.c_str(), name_size - 1);
    name[name_size - 1] = 0;
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_close_dir(uint8_t slot) override { return FsError::FS_ERROR_NONE; }
  FsError fs_remove(const char *path) override {
    return this->files.erase(path) ? FsError::FS_ERROR_NONE : FsError::FS_ERROR_NOT_FOUND;
  }
  FsError fs_rename(const char *from, const char *to) override { return FsError::FS_ERROR_INVALID; }
  FsError fs_stat(const char *path, FileInfo &info) override {
    auto it = this->files.find(path);
    if (it == this->files.end())
      return FsError::FS_ERROR_NOT_FOUND;
    info.size = it->second.size();
    info.is_dir = false;
    return FsError::FS_ERROR_NONE;
  }
  FsError fs_mkdir(const char *path) override { return FsError::FS_ERROR_NONE; }
  FsError fs_space(uint64_t &total, uint64_t &free) override {
    total = free = 0;
    return FsError::FS_ERROR_NONE;
  }

  std::vector<Open> open_;
  std::vector<size_t> dir_pos_;
};

}  // namespace esphome::filesystem::testing
