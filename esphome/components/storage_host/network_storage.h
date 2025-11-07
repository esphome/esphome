#pragma once

#include "esphome/core/component.h"
#include <string>
#include <vector>
#include <cstdint>

namespace esphome {
namespace storage_host {

/**
 * @brief Abstract base class for network storage devices (NFS, SMB, etc.)
 *
 * Provides a unified interface for network-based file storage systems.
 * Components implementing this interface can be registered with storage_host
 * to provide transparent network file access.
 *
 * Features:
 * - File operations (read, write, delete, exists)
 * - Directory operations (list, create, delete)
 * - Mount point management
 * - Connection state tracking
 */
class NetworkStorage {
 public:
  virtual ~NetworkStorage() = default;

  //========================================================================
  // Connection Management
  //========================================================================

  /**
   * @brief Check if connected to the network storage server
   * @return true if connected, false otherwise
   */
  virtual bool is_connected() const = 0;

  /**
   * @brief Get the mount path for this network storage
   * @return Mount path (e.g., "/nfs/myserver", "/smb/windows")
   */
  virtual const std::string &get_mount_path() const = 0;

  /**
   * @brief Get the storage protocol type
   * @return Protocol type (e.g., "nfs", "smb", "ftp")
   */
  virtual const char *get_protocol() const = 0;

  //========================================================================
  // File Operations
  //========================================================================

  /**
   * @brief Read entire file into memory
   * @param path File path relative to mount point or absolute
   * @param data Output buffer for file data
   * @return true if successful, false on error
   */
  virtual bool read_file(const std::string &path, std::vector<uint8_t> &data) = 0;

  /**
   * @brief Write data to file (overwrites existing file)
   * @param path File path relative to mount point or absolute
   * @param data Data to write
   * @param length Length of data in bytes
   * @return true if successful, false on error
   */
  virtual bool write_file(const std::string &path, const uint8_t *data, size_t length) = 0;

  /**
   * @brief Delete a file
   * @param path File path relative to mount point or absolute
   * @return true if successful, false on error
   */
  virtual bool delete_file(const std::string &path) = 0;

  /**
   * @brief Check if file exists
   * @param path File path relative to mount point or absolute
   * @return true if file exists, false otherwise
   */
  virtual bool file_exists(const std::string &path) = 0;

  //========================================================================
  // Directory Operations
  //========================================================================

  /**
   * @brief Directory entry structure (protocol-agnostic)
   */
  struct DirEntry {
    std::string name;
    uint64_t size{0};
    bool is_directory{false};
  };

  /**
   * @brief List directory contents
   * @param path Directory path relative to mount point or absolute
   * @param entries Output vector of directory entries
   * @return true if successful, false on error
   */
  virtual bool list_directory(const std::string &path, std::vector<DirEntry> &entries) = 0;

  /**
   * @brief Create a directory
   * @param path Directory path relative to mount point or absolute
   * @return true if successful, false on error
   */
  virtual bool create_directory(const std::string &path) = 0;

  /**
   * @brief Delete a directory (must be empty)
   * @param path Directory path relative to mount point or absolute
   * @return true if successful, false on error
   */
  virtual bool delete_directory(const std::string &path) = 0;

 protected:
  /**
   * @brief Helper to strip mount point prefix from path
   * @param path Absolute path
   * @param mount_path Mount point prefix
   * @return Relative path within the mount
   */
  static std::string strip_mount_prefix(const std::string &path, const std::string &mount_path) {
    if (path.compare(0, mount_path.length(), mount_path) == 0) {
      std::string relative = path.substr(mount_path.length());
      // Remove leading slash if present
      if (!relative.empty() && relative[0] == '/') {
        relative = relative.substr(1);
      }
      return relative.empty() ? "/" : "/" + relative;
    }
    return path;
  }
};

}  // namespace storage_host
}  // namespace esphome
