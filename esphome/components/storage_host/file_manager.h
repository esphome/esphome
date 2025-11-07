#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <ctime>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace storage_host {

// Forward declarations
class StorageHost;
class FileManager;

// =====================================================
// File Information Structures
// =====================================================

/// File metadata structure passed to automation triggers
struct FileInfo {
  std::string path;          // Full path to file
  std::string filename;      // Just the filename
  std::string directory;     // Parent directory
  uint64_t size{0};          // File size in bytes
  time_t modified_time{0};   // Last modification timestamp
  bool is_directory{false};  // True if this is a directory
};

/// Directory change information passed to automation triggers
struct DirectoryChangeInfo {
  std::string path;                // Directory path
  size_t file_count{0};            // Total files in directory (matching filters)
  size_t total_size{0};            // Total size of all files
  std::vector<FileInfo> files;     // All files in directory (matching filters)
  std::vector<FileInfo> added;     // Files added since last scan
  std::vector<FileInfo> modified;  // Files modified since last scan
  std::vector<FileInfo> deleted;   // Files deleted since last scan
};

// =====================================================
// Automation Triggers
// =====================================================

class FileAddedTrigger : public Trigger<FileInfo> {
 public:
  explicit FileAddedTrigger(FileManager *parent) : parent_(parent) {}

 protected:
  FileManager *parent_;
};

class FileModifiedTrigger : public Trigger<FileInfo> {
 public:
  explicit FileModifiedTrigger(FileManager *parent) : parent_(parent) {}

 protected:
  FileManager *parent_;
};

class FileDeletedTrigger : public Trigger<FileInfo> {
 public:
  explicit FileDeletedTrigger(FileManager *parent) : parent_(parent) {}

 protected:
  FileManager *parent_;
};

class DirectoryChangedTrigger : public Trigger<DirectoryChangeInfo> {
 public:
  explicit DirectoryChangedTrigger(FileManager *parent) : parent_(parent) {}

 protected:
  FileManager *parent_;
};

// =====================================================
// FileManager Component
// =====================================================

class FileManager : public Component {
 public:
  FileManager() = default;

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // =====================================================
  // Configuration
  // =====================================================

  void set_storage_host(StorageHost *storage) { this->storage_host_ = storage; }
  void set_watch_directory(const std::string &path) { this->watch_directory_ = path; }
  void set_watch_file(const std::string &path) { this->watch_file_ = path; }
  void set_scan_interval(uint32_t interval_ms) { this->scan_interval_ms_ = interval_ms; }

  // Pattern filters (glob patterns like "*.jpg", "*.png")
  void add_pattern(const std::string &pattern) { this->patterns_.push_back(pattern); }

  // Size filters
  void set_min_size(uint64_t size) { this->min_size_ = size; }
  void set_max_size(uint64_t size) { this->max_size_ = size; }

  // Trigger registration (for automation)
  void add_on_file_added_callback(FileAddedTrigger *trigger) { this->file_added_triggers_.push_back(trigger); }
  void add_on_file_modified_callback(FileModifiedTrigger *trigger) { this->file_modified_triggers_.push_back(trigger); }
  void add_on_file_deleted_callback(FileDeletedTrigger *trigger) { this->file_deleted_triggers_.push_back(trigger); }

  // Callback registration (for lambdas)
  void add_on_file_added_callback(std::function<void(FileInfo)> &&callback) {
    this->file_added_callbacks_.add(std::move(callback));
  }
  void add_on_file_modified_callback(std::function<void(FileInfo)> &&callback) {
    this->file_modified_callbacks_.add(std::move(callback));
  }
  void add_on_file_deleted_callback(std::function<void(FileInfo)> &&callback) {
    this->file_deleted_callbacks_.add(std::move(callback));
  }
  void add_on_directory_changed_callback(DirectoryChangedTrigger *trigger) {
    this->directory_changed_triggers_.push_back(trigger);
  }
  void add_on_directory_changed_callback(std::function<void(DirectoryChangeInfo)> &&callback) {
    this->directory_changed_callbacks_.add(std::move(callback));
  }

  // =====================================================
  // Safe File Operations
  // =====================================================

  /// Read file with mount availability check
  bool read_file_safe(const std::string &path, std::vector<uint8_t> &data);

  /// Write file with mount availability check
  bool write_file_safe(const std::string &path, const uint8_t *data, size_t length);

  /// Check if file exists with mount availability check
  bool file_exists_safe(const std::string &path);

  /// Check if path is currently accessible (mount available)
  bool is_path_available(const std::string &path);

  // =====================================================
  // Manual Operations
  // =====================================================

  /// Force a scan now (useful for testing or manual refresh)
  void scan_now();

  /// Get current directory state (for debugging)
  const std::map<std::string, FileInfo> &get_directory_state() const { return this->directory_state_; }

 protected:
  // Configuration
  StorageHost *storage_host_{nullptr};
  std::string watch_directory_;
  std::string watch_file_;
  uint32_t scan_interval_ms_{5000};  // Default: scan every 5 seconds
  std::vector<std::string> patterns_;
  uint64_t min_size_{0};
  uint64_t max_size_{UINT64_MAX};

  // State tracking
  std::map<std::string, FileInfo> directory_state_;  // Current known files (key: full path)
  uint32_t last_scan_time_{0};
  bool initial_scan_done_{false};

  // Automation triggers
  std::vector<FileAddedTrigger *> file_added_triggers_;
  std::vector<FileModifiedTrigger *> file_modified_triggers_;
  std::vector<FileDeletedTrigger *> file_deleted_triggers_;
  std::vector<DirectoryChangedTrigger *> directory_changed_triggers_;

  // Callbacks
  CallbackManager<void(FileInfo)> file_added_callbacks_;
  CallbackManager<void(FileInfo)> file_modified_callbacks_;
  CallbackManager<void(FileInfo)> file_deleted_callbacks_;
  CallbackManager<void(DirectoryChangeInfo)> directory_changed_callbacks_;

  // =====================================================
  // Internal Methods
  // =====================================================

  /// Perform a directory scan and detect changes
  void perform_scan_();

  /// Scan a single directory
  void scan_directory_(const std::string &path, std::vector<FileInfo> &files);

  /// Check if a filename matches any of the configured patterns
  bool matches_pattern_(const std::string &filename) const;

  /// Check if a file passes size filters
  bool passes_filters_(const FileInfo &info) const;

  /// Compare two file states to detect modifications
  bool file_modified_(const FileInfo &old_info, const FileInfo &new_info) const;

  /// Create FileInfo from file path
  bool get_file_info_(const std::string &path, FileInfo &info);

  /// Extract filename from full path
  std::string get_filename_(const std::string &path) const;

  /// Extract directory from full path
  std::string get_directory_(const std::string &path) const;
};

}  // namespace storage_host
}  // namespace esphome
