#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstring>
#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include "network_storage.h"

namespace esphome {

// Forward declaration for binary_storage (soft dependency)
namespace binary_storage {
class BinaryStorage;
}

namespace storage_host {

// Forward declarations
class StorageHost;

// Mount point entry - lightweight alternative to std::map
struct MountEntry {
  std::string path;
  std::string platform;
};

// =====================================================
// StorageMount - Individual mount point reference
// =====================================================
class StorageMount {
 public:
  StorageMount() = default;

  void set_path(const std::string &path) { this->path_ = path; }
  void set_platform(const std::string &platform) { this->platform_ = platform; }
  void set_storage_host(StorageHost *host) { this->storage_host_ = host; }

  const std::string &get_path() const { return this->path_; }
  const std::string &get_platform() const { return this->platform_; }
  StorageHost *get_storage_host() const { return this->storage_host_; }

  /// Check if this mount point is available (e.g., SD card inserted)
  bool is_available() const;

  /// Get mount statistics (if supported by platform)
  bool get_stats(uint64_t &total_bytes, uint64_t &free_bytes) const;

 protected:
  std::string path_;
  std::string platform_;
  StorageHost *storage_host_{nullptr};
};

// Device node entry - virtual /dev files pointing to binary_storage devices
struct DeviceNode {
  std::string path;                       // e.g., "/dev/fram0"
  binary_storage::BinaryStorage *device;  // Pointer to binary_storage device
  std::string device_type;                // e.g., "i2c_fram", "spi_flash"
};

// Maximum number of mount points (SD, USB, internal, etc.)
static constexpr size_t MAX_MOUNT_POINTS = 8;

// Maximum number of device nodes (binary_storage devices in /dev)
static constexpr size_t MAX_DEVICE_NODES = 16;

// Maximum number of network storage backends (NFS, SMB, FTP, etc.)
static constexpr size_t MAX_NETWORK_STORAGE = 4;

// =====================================================
// StorageHost - Main Storage Class
// =====================================================
class StorageHost : public Component {
 public:
  StorageHost() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // File methods
  bool file_exists(const std::string &path);
  std::string read_file(const std::string &path);
  bool write_file(const std::string &path, const std::string &data);
  std::vector<std::string> list_files(const std::string &path);

  // Mount management
  void register_mount(const std::string &path, const std::string &platform);
  const esphome::StaticVector<MountEntry, MAX_MOUNT_POINTS> &get_mounts() const { return this->mounts_; }
  std::string find_mount_for_path(const std::string &path);

  // Device node management (virtual /dev files for binary_storage)
  void register_device_node(const std::string &path, binary_storage::BinaryStorage *device,
                             const std::string &device_type);
  const esphome::StaticVector<DeviceNode, MAX_DEVICE_NODES> &get_device_nodes() const {
    return this->device_nodes_;
  }
  bool is_device_node(const std::string &path) const;
  DeviceNode *find_device_node(const std::string &path);

  // Network storage management (NFS, SMB, FTP, etc.)
  void register_network_storage(NetworkStorage *storage);
  const esphome::StaticVector<NetworkStorage *, MAX_NETWORK_STORAGE> &get_network_storage() const {
    return this->network_storage_;
  }
  NetworkStorage *find_network_storage_for_path(const std::string &path);
  bool is_network_path(const std::string &path) const;

  // Network storage file operations (use network backend if available, fallback to POSIX)
  bool network_file_exists(const std::string &path);
  bool network_read_file(const std::string &path, std::vector<uint8_t> &data);
  bool network_write_file(const std::string &path, const uint8_t *data, size_t length);
  bool network_delete_file(const std::string &path);
  bool network_list_directory(const std::string &path, std::vector<NetworkStorage::DirEntry> &entries);
  bool network_create_directory(const std::string &path);
  bool network_delete_directory(const std::string &path);

 protected:
  esphome::StaticVector<MountEntry, MAX_MOUNT_POINTS> mounts_;
  esphome::StaticVector<DeviceNode, MAX_DEVICE_NODES> device_nodes_;
  esphome::StaticVector<NetworkStorage *, MAX_NETWORK_STORAGE> network_storage_;
};

// Global accessor for soft dependency pattern
extern StorageHost *global_storage_host;

}  // namespace storage_host
}  // namespace esphome
