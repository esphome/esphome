#include "file_manager.h"
#include "storage_host.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>

namespace esphome {
namespace storage_host {

static const char *const TAG = "file_manager";

// =====================================================
// Component Lifecycle
// =====================================================

void FileManager::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FileManager...");

  if (this->storage_host_ == nullptr) {
    ESP_LOGE(TAG, "StorageHost not set!");
    this->mark_failed();
    return;
  }

  // Validate configuration
  if (this->watch_directory_.empty() && this->watch_file_.empty()) {
    ESP_LOGE(TAG, "Neither watch_directory nor watch_file is set!");
    this->mark_failed();
    return;
  }

  if (!this->watch_directory_.empty() && !this->watch_file_.empty()) {
    ESP_LOGW(TAG, "Both watch_directory and watch_file set, using watch_directory");
    this->watch_file_.clear();
  }

  // If no patterns specified, match all files
  if (this->patterns_.empty() && !this->watch_directory_.empty()) {
    this->patterns_.push_back("*");
  }

  ESP_LOGCONFIG(TAG, "FileManager setup complete");
}

void FileManager::loop() {
  uint32_t now = millis();

  // Check if it's time to scan
  if (now - this->last_scan_time_ >= this->scan_interval_ms_) {
    this->perform_scan_();
    this->last_scan_time_ = now;
  }
}

void FileManager::dump_config() {
  ESP_LOGCONFIG(TAG, "FileManager:");

  if (!this->watch_directory_.empty()) {
    ESP_LOGCONFIG(TAG, "  Watch Directory: %s", this->watch_directory_.c_str());
  }
  if (!this->watch_file_.empty()) {
    ESP_LOGCONFIG(TAG, "  Watch File: %s", this->watch_file_.c_str());
  }

  ESP_LOGCONFIG(TAG, "  Scan Interval: %u ms", this->scan_interval_ms_);

  if (!this->patterns_.empty()) {
    ESP_LOGCONFIG(TAG, "  Patterns:");
    for (const auto &pattern : this->patterns_) {
      ESP_LOGCONFIG(TAG, "    - %s", pattern.c_str());
    }
  }

  if (this->min_size_ > 0 || this->max_size_ < UINT64_MAX) {
    ESP_LOGCONFIG(TAG, "  Size Filter: %llu - %llu bytes", this->min_size_, this->max_size_);
  }

  ESP_LOGCONFIG(TAG, "  Callbacks:");
  ESP_LOGCONFIG(TAG, "    - on_file_added: %s", this->file_added_callbacks_.size() > 0 ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "    - on_file_modified: %s", this->file_modified_callbacks_.size() > 0 ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "    - on_file_deleted: %s", this->file_deleted_callbacks_.size() > 0 ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "    - on_directory_changed: %s",
                this->directory_changed_callbacks_.size() > 0 ? "yes" : "no");
}

// =====================================================
// Safe File Operations
// =====================================================

bool FileManager::is_path_available(const std::string &path) {
  if (this->storage_host_ == nullptr) {
    return false;
  }

  // Check if path is on a network storage
  if (this->storage_host_->is_network_path(path)) {
    auto *network_storage = this->storage_host_->find_network_storage_for_path(path);
    return network_storage != nullptr && network_storage->is_connected();
  }

  // For local paths, check if mount point exists in StorageHost configuration
  std::string mount_point = this->storage_host_->find_mount_for_path(path);
  if (mount_point.empty()) {
    ESP_LOGD(TAG, "No mount point found for path: %s", path.c_str());
    return false;
  }

  // For virtual mount points (ESP-IDF VFS), stat() may fail even when mount is valid
  // Instead, try to actually access the path - opendir() will fail if not accessible
  // This avoids false negatives when mount points are virtual and not stat-able
  ESP_LOGV(TAG, "Mount point '%s' configured for path '%s', assuming available", mount_point.c_str(), path.c_str());
  return true;  // Let scan_directory_() handle actual accessibility check via opendir()
}

bool FileManager::read_file_safe(const std::string &path, std::vector<uint8_t> &data) {
  if (!this->is_path_available(path)) {
    ESP_LOGW(TAG, "Path not available: %s", path.c_str());
    return false;
  }

  // Check if it's a network path
  if (this->storage_host_->is_network_path(path)) {
    return this->storage_host_->network_read_file(path, data);
  }

  // Local file access
  return this->storage_host_->file_exists(path);
}

bool FileManager::write_file_safe(const std::string &path, const uint8_t *data, size_t length) {
  if (!this->is_path_available(path)) {
    ESP_LOGW(TAG, "Path not available: %s", path.c_str());
    return false;
  }

  // Check if it's a network path
  if (this->storage_host_->is_network_path(path)) {
    return this->storage_host_->network_write_file(path, data, length);
  }

  // Local file access
  return this->storage_host_->write_file(path, std::string((const char *) data, length));
}

bool FileManager::file_exists_safe(const std::string &path) {
  if (!this->is_path_available(path)) {
    return false;
  }

  // Check if it's a network path
  if (this->storage_host_->is_network_path(path)) {
    return this->storage_host_->network_file_exists(path);
  }

  // Local file access
  return this->storage_host_->file_exists(path);
}

// =====================================================
// Scanning & Change Detection
// =====================================================

void FileManager::scan_now() { this->perform_scan_(); }

void FileManager::perform_scan_() {
  // Single file watch mode
  if (!this->watch_file_.empty()) {
    // Get mount point for single file
    std::string mount_point = this->storage_host_ ? this->storage_host_->find_mount_for_path(this->watch_file_) : "";
    FileInfo info;
    bool exists = this->get_file_info_(this->watch_file_, mount_point, info);

    auto it = this->directory_state_.find(this->watch_file_);
    bool was_tracked = it != this->directory_state_.end();

    if (exists && !was_tracked) {
      // File added
      ESP_LOGD(TAG, "File added: %s", this->watch_file_.c_str());
      this->directory_state_[this->watch_file_] = info;
      this->file_added_callbacks_.call(info);

    } else if (exists && was_tracked) {
      // Check if modified
      if (this->file_modified_(it->second, info)) {
        ESP_LOGD(TAG, "File modified: %s", this->watch_file_.c_str());
        this->directory_state_[this->watch_file_] = info;
        this->file_modified_callbacks_.call(info);
      }

    } else if (!exists && was_tracked) {
      // File deleted
      ESP_LOGD(TAG, "File deleted: %s", this->watch_file_.c_str());
      FileInfo deleted_info = it->second;
      this->directory_state_.erase(it);
      this->file_deleted_callbacks_.call(deleted_info);
    }

    return;
  }

  // Directory watch mode
  if (!this->watch_directory_.empty()) {
    // Scan directory (scan_directory_() will handle mount point validation and graceful failure)
    std::vector<FileInfo> current_files;
    this->scan_directory_(this->watch_directory_, current_files);

    // Detect changes
    DirectoryChangeInfo change_info;
    change_info.path = this->watch_directory_;
    change_info.file_count = current_files.size();

    // Build map of current files for efficient lookup
    std::map<std::string, FileInfo> current_map;
    for (const auto &file : current_files) {
      current_map[file.path] = file;
      change_info.total_size += file.size;
    }

    // Detect added and modified files
    for (const auto &[path, info] : current_map) {
      auto it = this->directory_state_.find(path);

      if (it == this->directory_state_.end()) {
        // File added
        if (this->initial_scan_done_) {
          ESP_LOGD(TAG, "File added: %s", info.filename.c_str());
          change_info.added.push_back(info);
        }
      } else {
        // Check if modified
        if (this->file_modified_(it->second, info)) {
          ESP_LOGD(TAG, "File modified: %s", info.filename.c_str());
          change_info.modified.push_back(info);
        }
      }
    }

    // Detect deleted files
    for (const auto &[path, info] : this->directory_state_) {
      if (current_map.find(path) == current_map.end()) {
        // File deleted
        if (this->initial_scan_done_) {
          ESP_LOGD(TAG, "File deleted: %s", info.filename.c_str());
          change_info.deleted.push_back(info);
        }
      }
    }

    // Update state BEFORE firing callbacks so callbacks see the new state
    this->directory_state_ = current_map;
    change_info.files = current_files;

    // Fire individual file callbacks AFTER state is updated
    if (this->initial_scan_done_) {
      for (const auto &info : change_info.added) {
        this->file_added_callbacks_.call(info);
        for (auto *trigger : this->file_added_triggers_) {
          trigger->trigger(info);
        }
      }

      for (const auto &info : change_info.modified) {
        this->file_modified_callbacks_.call(info);
        for (auto *trigger : this->file_modified_triggers_) {
          trigger->trigger(info);
        }
      }

      for (const auto &info : change_info.deleted) {
        this->file_deleted_callbacks_.call(info);
        for (auto *trigger : this->file_deleted_triggers_) {
          trigger->trigger(info);
        }
      }
    }

    // Fire directory changed callback if any changes occurred OR on first scan
    bool has_changes = (!change_info.added.empty() || !change_info.modified.empty() || !change_info.deleted.empty());
    if (!this->initial_scan_done_) {
      // First scan - notify with initial file list (even if empty)
      ESP_LOGD(TAG, "Initial directory scan complete: %s (%zu files)", this->watch_directory_.c_str(),
               change_info.file_count);
      this->directory_changed_callbacks_.call(change_info);
      for (auto *trigger : this->directory_changed_triggers_) {
        trigger->trigger(change_info);
      }
    } else if (has_changes) {
      // Subsequent scans - only notify if changes detected
      ESP_LOGD(TAG, "Directory changed: %s (added: %zu, modified: %zu, deleted: %zu)", this->watch_directory_.c_str(),
               change_info.added.size(), change_info.modified.size(), change_info.deleted.size());
      this->directory_changed_callbacks_.call(change_info);
      for (auto *trigger : this->directory_changed_triggers_) {
        trigger->trigger(change_info);
      }
    }

    this->initial_scan_done_ = true;
  }
}

void FileManager::scan_directory_(const std::string &path, std::vector<FileInfo> &files) {
  if (this->storage_host_ == nullptr) {
    ESP_LOGD(TAG, "StorageHost not available");
    return;
  }

  // Check if this is virtual root - list mount points
  bool is_virtual_root = (path.empty() || path == "/");
  if (is_virtual_root) {
    ESP_LOGD(TAG, "Listing mount points from virtual root");
    const auto &mounts = this->storage_host_->get_mounts();
    for (const auto &mount : mounts) {
      FileInfo info;
      info.path = mount.path;
      info.filename = mount.path.substr(1);  // Remove leading slash
      if (info.filename.empty()) {
        info.filename = "root";
      }
      info.directory = "/";
      info.size = 0;
      info.modified_time = 0;
      info.is_directory = true;

      // Check if pattern matches (mount points should match "*" pattern)
      if (this->matches_pattern_(info.filename)) {
        files.push_back(info);
      }
    }
    return;
  }

  // Parse path to extract mount point
  std::string mount_point = this->storage_host_->find_mount_for_path(path);
  if (mount_point.empty()) {
    ESP_LOGD(TAG, "No mount point found for path: %s", path.c_str());
    return;
  }

  ESP_LOGV(TAG, "Found mount point '%s' for path '%s'", mount_point.c_str(), path.c_str());

  // Check if this is a network path - use StorageHost methods for proper routing
  if (this->storage_host_->is_network_path(path)) {
    // Use network storage backend
    std::vector<NetworkStorage::DirEntry> entries;
    if (!this->storage_host_->network_list_directory(path, entries)) {
      ESP_LOGD(TAG, "Failed to list network directory: %s", path.c_str());
      return;
    }

    for (const auto &entry : entries) {
      // Check pattern matching
      if (!this->matches_pattern_(entry.name)) {
        continue;
      }

      // Build FileInfo from network entry
      FileInfo info;
      // Build full path from directory path + filename
      info.path = path;
      if (!info.path.empty() && info.path.back() != '/') {
        info.path += '/';
      }
      info.path += entry.name;
      info.filename = entry.name;
      info.directory = path;
      info.size = entry.size;
      info.modified_time = 0;  // Network entries don't provide modification time
      info.is_directory = entry.is_directory;

      // Check filters
      if (this->passes_filters_(info)) {
        files.push_back(info);
      }
    }
    return;
  }

  // Local filesystem access - use opendir()
  DIR *dir = opendir(path.c_str());
  if (dir == nullptr) {
    ESP_LOGD(TAG, "Failed to open directory: %s (errno: %d, mount: %s)", path.c_str(), errno, mount_point.c_str());
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    std::string filename = entry->d_name;

    // Check pattern matching
    if (!this->matches_pattern_(filename)) {
      continue;
    }

    // Build full path
    std::string full_path = path;
    if (full_path.back() != '/') {
      full_path += '/';
    }
    full_path += filename;

    // Get file info (pass mount point for VFS routing)
    FileInfo info;
    if (this->get_file_info_(full_path, mount_point, info)) {
      // Check filters
      if (this->passes_filters_(info)) {
        files.push_back(info);
      }
    }
  }

  closedir(dir);
}

bool FileManager::get_file_info_(const std::string &path, const std::string &mount_point, FileInfo &info) {
  // Strip mount point prefix to get VFS-relative path
  // Example: path="/usb/jpg/3.jpg", mount="/usb" -> vfs_path="/jpg/3.jpg"
  std::string vfs_path = path;
  if (!mount_point.empty() && path.compare(0, mount_point.length(), mount_point) == 0) {
    vfs_path = path.substr(mount_point.length());
    // Ensure path starts with /
    if (vfs_path.empty() || vfs_path[0] != '/') {
      vfs_path = "/" + vfs_path;
    }
  }

  struct stat st;
  if (stat(vfs_path.c_str(), &st) != 0) {
    ESP_LOGD(TAG, "stat() failed for vfs_path: '%s' (full: '%s', mount: '%s', errno: %d)",
             vfs_path.c_str(), path.c_str(), mount_point.c_str(), errno);
    return false;
  }

  info.path = path;  // Store full path with mount point
  info.filename = this->get_filename_(path);
  info.directory = this->get_directory_(path);
  info.size = st.st_size;
  info.modified_time = st.st_mtime;
  info.is_directory = S_ISDIR(st.st_mode);

  ESP_LOGV(TAG, "stat('%s'): size=%llu, is_dir=%d (vfs_path='%s', mount='%s')",
           path.c_str(), (unsigned long long)st.st_size, S_ISDIR(st.st_mode), vfs_path.c_str(), mount_point.c_str());

  return true;
}

// =====================================================
// Pattern Matching & Filtering
// =====================================================

bool FileManager::matches_pattern_(const std::string &filename) const {
  if (this->patterns_.empty()) {
    return true;
  }

  for (const auto &pattern : this->patterns_) {
    // Simple glob matching: * matches any sequence, ? matches single character
    size_t pat_idx = 0;
    size_t name_idx = 0;
    size_t star_pat_idx = std::string::npos;
    size_t star_name_idx = std::string::npos;

    while (name_idx < filename.length()) {
      if (pat_idx < pattern.length() && (pattern[pat_idx] == filename[name_idx] || pattern[pat_idx] == '?')) {
        // Character match or ? wildcard
        pat_idx++;
        name_idx++;
      } else if (pat_idx < pattern.length() && pattern[pat_idx] == '*') {
        // * wildcard - remember position
        star_pat_idx = pat_idx;
        star_name_idx = name_idx;
        pat_idx++;
      } else if (star_pat_idx != std::string::npos) {
        // Backtrack to last * and try matching more characters
        pat_idx = star_pat_idx + 1;
        star_name_idx++;
        name_idx = star_name_idx;
      } else {
        // No match
        break;
      }
    }

    // Consume remaining * in pattern
    while (pat_idx < pattern.length() && pattern[pat_idx] == '*') {
      pat_idx++;
    }

    // If we've consumed entire pattern and filename, it's a match
    if (pat_idx == pattern.length() && name_idx == filename.length()) {
      return true;
    }
  }

  return false;
}

bool FileManager::passes_filters_(const FileInfo &info) const {
  // Don't filter directories (for now)
  if (info.is_directory) {
    return false;  // Skip directories in file listing
  }

  // Size filters
  if (info.size < this->min_size_ || info.size > this->max_size_) {
    return false;
  }

  return true;
}

bool FileManager::file_modified_(const FileInfo &old_info, const FileInfo &new_info) const {
  // Check if size or modification time changed
  return old_info.size != new_info.size || old_info.modified_time != new_info.modified_time;
}

// =====================================================
// Utility Methods
// =====================================================

std::string FileManager::get_filename_(const std::string &path) const {
  size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

std::string FileManager::get_directory_(const std::string &path) const {
  size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return "";
  }
  return path.substr(0, pos);
}

}  // namespace storage_host
}  // namespace esphome
