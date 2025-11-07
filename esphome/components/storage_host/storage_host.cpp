#include "storage_host.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"  // For App.feed_wdt()
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <errno.h>
#include <algorithm>

// Include yield function for ESP32/ESP8266
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define yield() taskYIELD()
#elif defined(ESP8266)
#include <Esp.h>
// yield() is already available on ESP8266
#else
// Fallback for other platforms
#define yield() delayMicroseconds(1)
#endif

namespace esphome {
namespace storage_host {

static const char *const TAG = "storage_host";

// Global accessor for soft dependency pattern
StorageHost *global_storage_host = nullptr;

// =====================================================
// StorageMount Implementation
// =====================================================

bool StorageMount::is_available() const {
  if (this->storage_host_ == nullptr) {
    return false;
  }

  // Check if the mount path exists
  struct stat st;
  if (stat(this->path_.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }

  return false;
}

bool StorageMount::get_stats(uint64_t &total_bytes, uint64_t &free_bytes) const {
#ifdef ESP32
  // ESP32/ESP-IDF has statvfs
  struct statvfs stat;
  if (statvfs(this->path_.c_str(), &stat) == 0) {
    total_bytes = static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
    free_bytes = static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
    return true;
  }
#endif
  return false;
}

// =====================================================
// StorageHost Implementation
// =====================================================

void StorageHost::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Storage Host Component...");

  // Set global accessor for soft dependency pattern
  global_storage_host = this;

  ESP_LOGCONFIG(TAG, "  Mounts configured: %zu", this->mounts_.size());
  for (const auto &mount : this->mounts_) {
    ESP_LOGCONFIG(TAG, "    - %s (platform: %s)", mount.path.c_str(), mount.platform.c_str());
  }

  ESP_LOGCONFIG(TAG, "  Device nodes configured: %zu", this->device_nodes_.size());
  for (const auto &node : this->device_nodes_) {
    ESP_LOGCONFIG(TAG, "    - %s (type: %s)", node.path.c_str(), node.device_type.c_str());
  }
}

void StorageHost::loop() {
  // Nothing to do in loop
}

void StorageHost::dump_config() {
  ESP_LOGCONFIG(TAG, "Storage Host Component:");
  ESP_LOGCONFIG(TAG, "  Mounts: %zu", this->mounts_.size());
  for (const auto &mount : this->mounts_) {
    ESP_LOGCONFIG(TAG, "    - %s (platform: %s)", mount.path.c_str(), mount.platform.c_str());
  }
  ESP_LOGCONFIG(TAG, "  Device Nodes (/dev): %zu", this->device_nodes_.size());
  for (const auto &node : this->device_nodes_) {
    ESP_LOGCONFIG(TAG, "    - %s (type: %s)", node.path.c_str(), node.device_type.c_str());
  }
  ESP_LOGCONFIG(TAG, "  Network Storage: %zu", this->network_storage_.size());
  for (const auto *storage : this->network_storage_) {
    ESP_LOGCONFIG(TAG, "    - %s (protocol: %s, connected: %s)", storage->get_mount_path().c_str(),
                  storage->get_protocol(), storage->is_connected() ? "yes" : "no");
  }
}

void StorageHost::register_mount(const std::string &path, const std::string &platform) {
  this->mounts_.push_back({path, platform});
  ESP_LOGD(TAG, "Registered mount: %s (platform: %s)", path.c_str(), platform.c_str());
}

std::string StorageHost::find_mount_for_path(const std::string &path) {
  // Find the longest matching mount point
  std::string best_mount;
  size_t best_length = 0;

  for (const auto &mount : this->mounts_) {
    if (path.compare(0, mount.path.length(), mount.path) == 0) {
      if (mount.path.length() > best_length) {
        best_mount = mount.path;
        best_length = mount.path.length();
      }
    }
  }

  return best_mount;
}

// =====================================================
// Device Node Management
// =====================================================

void StorageHost::register_device_node(const std::string &path, binary_storage::BinaryStorage *device,
                                       const std::string &device_type) {
  this->device_nodes_.push_back({path, device, device_type});
  ESP_LOGD(TAG, "Registered device node: %s (type: %s, device: %p)", path.c_str(), device_type.c_str(), device);
}

bool StorageHost::is_device_node(const std::string &path) const {
  for (const auto &node : this->device_nodes_) {
    if (node.path == path) {
      return true;
    }
  }
  return false;
}

DeviceNode *StorageHost::find_device_node(const std::string &path) {
  for (auto &node : this->device_nodes_) {
    if (node.path == path) {
      return &node;
    }
  }
  return nullptr;
}

// =====================================================
// Network Storage Management
// =====================================================

void StorageHost::register_network_storage(NetworkStorage *storage) {
  if (storage == nullptr) {
    ESP_LOGW(TAG, "Attempted to register null network storage");
    return;
  }

  this->network_storage_.push_back(storage);
  ESP_LOGD(TAG, "Registered network storage: %s (protocol: %s, mount: %s)", storage->get_protocol(),
           storage->get_protocol(), storage->get_mount_path().c_str());
}

NetworkStorage *StorageHost::find_network_storage_for_path(const std::string &path) {
  // Find the network storage with the longest matching mount path
  NetworkStorage *best_match = nullptr;
  size_t best_length = 0;

  for (auto *storage : this->network_storage_) {
    const std::string &mount_path = storage->get_mount_path();
    if (path.compare(0, mount_path.length(), mount_path) == 0) {
      if (mount_path.length() > best_length) {
        best_match = storage;
        best_length = mount_path.length();
      }
    }
  }

  return best_match;
}

bool StorageHost::is_network_path(const std::string &path) const {
  for (const auto *storage : this->network_storage_) {
    const std::string &mount_path = storage->get_mount_path();
    if (path.compare(0, mount_path.length(), mount_path) == 0) {
      return true;
    }
  }
  return false;
}

// =====================================================
// Network Storage File Operations
// =====================================================

bool StorageHost::network_file_exists(const std::string &path) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->file_exists(path);
  }
  return false;
}

bool StorageHost::network_read_file(const std::string &path, std::vector<uint8_t> &data) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->read_file(path, data);
  }
  ESP_LOGW(TAG, "No connected network storage found for path: %s", path.c_str());
  return false;
}

bool StorageHost::network_write_file(const std::string &path, const uint8_t *data, size_t length) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->write_file(path, data, length);
  }
  ESP_LOGW(TAG, "No connected network storage found for path: %s", path.c_str());
  return false;
}

bool StorageHost::network_delete_file(const std::string &path) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->delete_file(path);
  }
  ESP_LOGW(TAG, "No connected network storage found for path: %s", path.c_str());
  return false;
}

bool StorageHost::network_list_directory(const std::string &path, std::vector<NetworkStorage::DirEntry> &entries) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->list_directory(path, entries);
  }
  ESP_LOGW(TAG, "No connected network storage found for path: %s", path.c_str());
  return false;
}

bool StorageHost::network_create_directory(const std::string &path) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->create_directory(path);
  }
  ESP_LOGW(TAG, "No connected network storage found for path: %s", path.c_str());
  return false;
}

bool StorageHost::network_delete_directory(const std::string &path) {
  NetworkStorage *storage = this->find_network_storage_for_path(path);
  if (storage != nullptr && storage->is_connected()) {
    return storage->delete_directory(path);
  }
  ESP_LOGW(TAG, "No connected network storage found for path: %s", path.c_str());
  return false;
}

bool StorageHost::file_exists(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string StorageHost::read_file(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");

  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s (errno: %d)", path.c_str(), errno);
    return "";
  }

  // Get file size safely
  if (fseek(file, 0, SEEK_END) != 0) {
    ESP_LOGE(TAG, "Failed to seek to end of file: %s", path.c_str());
    fclose(file);
    return "";
  }

  long size = ftell(file);
  if (size < 0 || size > 10 * 1024 * 1024) {  // 10MB limit
    ESP_LOGE(TAG, "Invalid file size: %ld bytes", size);
    fclose(file);
    return "";
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to beginning of file: %s", path.c_str());
    fclose(file);
    return "";
  }

  std::string data(size, '\0');
  size_t read_size = fread(&data[0], 1, size, file);
  fclose(file);

  if (read_size != static_cast<size_t>(size)) {
    ESP_LOGE(TAG, "Failed to read complete file: expected %ld, got %zu", size, read_size);
    return "";
  }

  return data;
}

bool StorageHost::write_file(const std::string &path, const std::string &data) {
  FILE *file = fopen(path.c_str(), "wb");

  if (!file) {
    ESP_LOGE(TAG, "Failed to create file: %s", path.c_str());
    return false;
  }

  size_t written = fwrite(data.data(), 1, data.size(), file);
  fclose(file);

  return written == data.size();
}

std::vector<std::string> StorageHost::list_files(const std::string &path) {
  std::vector<std::string> files;

  DIR *dir = opendir(path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Cannot open directory: %s", path.c_str());
    return files;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG) {
      files.push_back(entry->d_name);
    }
  }

  closedir(dir);
  return files;
}


}  // namespace storage_host
}  // namespace esphome
