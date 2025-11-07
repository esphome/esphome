#pragma once

#ifdef USE_ESP_IDF

#include "binary_storage.h"
#include "esphome/core/component.h"
#include "esp_vfs.h"
#include "esp_littlefs.h"
#include <string>

namespace esphome {
namespace binary_storage {

/**
 * @brief LittleFS mount manager for binary storage devices
 *
 * Mounts BinaryStorage devices (FRAM, EEPROM, Flash) as LittleFS filesystems
 * in the ESP-IDF VFS (Virtual File System).
 *
 * Features:
 * - Auto-format on first mount (optional)
 * - Custom block device adapter for BinaryStorage
 * - Integrates with storage_host for unified access
 */
class LittleFSMount : public Component {
 public:
  LittleFSMount() = default;
  ~LittleFSMount() override;

  // Component lifecycle
  void setup() override;
  void loop() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA + 1; }

  //========================================================================
  // Configuration
  //========================================================================

  /**
   * @brief Set the binary storage device to mount
   *
   * @param storage Pointer to BinaryStorage device
   */
  void set_storage_device(BinaryStorage *storage) { this->storage_ = storage; }

  /**
   * @brief Set mount point path
   *
   * @param path VFS mount point (e.g., "/fram", "/eeprom")
   */
  void set_mount_path(const std::string &path) { this->mount_path_ = path; }

  /**
   * @brief Set whether to format on mount failure
   *
   * @param format If true, will format filesystem if mount fails
   */
  void set_auto_format(bool format) { this->auto_format_ = format; }

  /**
   * @brief Set custom partition label (optional)
   *
   * @param label Partition label for LittleFS
   */
  void set_partition_label(const std::string &label) { this->partition_label_ = label; }

  //========================================================================
  // Mount Management
  //========================================================================

  /**
   * @brief Check if filesystem is mounted
   *
   * @return true if mounted
   */
  bool is_mounted() const { return this->mounted_; }

  /**
   * @brief Get mount path
   *
   * @return Mount path string
   */
  const std::string &get_mount_path() const { return this->mount_path_; }

  /**
   * @brief Manually unmount filesystem
   *
   * @return true on success
   */
  bool unmount();

  /**
   * @brief Manually remount filesystem
   *
   * @return true on success
   */
  bool remount();

  /**
   * @brief Format the filesystem
   *
   * WARNING: This erases all data!
   *
   * @return true on success
   */
  bool format();

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  BinaryStorage *storage_{nullptr};
  std::string mount_path_{"/storage"};
  std::string partition_label_;
  bool auto_format_{true};
  bool mounted_{false};

  //========================================================================
  // Internal Helpers
  //========================================================================

  /**
   * @brief Mount the LittleFS filesystem
   *
   * @return true on success
   */
  bool mount_();

  /**
   * @brief Configure LittleFS for the storage device
   *
   * @return LittleFS configuration structure
   */
  esp_vfs_littlefs_conf_t get_littlefs_config_();

  /**
   * @brief Register with storage_host
   */
  void register_with_storage_host_();
};

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_ESP_IDF
