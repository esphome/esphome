#ifdef USE_ESP_IDF

#include "littlefs_mount.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

// Soft dependency on storage_host
#if defined(USE_STORAGE_HOST)
namespace storage_host {
extern class StorageHost *global_storage_host;
}
#endif  // USE_STORAGE_HOST

namespace esphome {
namespace binary_storage {

static const char *const TAG = "littlefs_mount";

LittleFSMount::~LittleFSMount() {
  if (this->mounted_) {
    this->unmount();
  }
}

void LittleFSMount::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LittleFS Mount...");

  if (this->storage_ == nullptr) {
    ESP_LOGE(TAG, "No storage device configured!");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "  Mount Path: %s", this->mount_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Storage Device: %s", this->storage_->get_device_name());
  ESP_LOGCONFIG(TAG, "  Auto Format: %s", this->auto_format_ ? "YES" : "NO");

  // Attempt to mount
  if (this->mount_()) {
    ESP_LOGI(TAG, "Successfully mounted LittleFS at %s", this->mount_path_.c_str());
    this->register_with_storage_host_();
  } else {
    ESP_LOGE(TAG, "Failed to mount LittleFS!");
    this->mark_failed();
  }
}

void LittleFSMount::dump_config() {
  ESP_LOGCONFIG(TAG, "LittleFS Mount:");
  ESP_LOGCONFIG(TAG, "  Mount Path: %s", this->mount_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Device: %s (%s)", this->storage_->get_device_name(), this->storage_->get_device_type());
  ESP_LOGCONFIG(TAG, "  Mounted: %s", this->mounted_ ? "YES" : "NO");

  if (this->mounted_) {
    // Get filesystem info
    size_t total_bytes = 0, used_bytes = 0;
    esp_err_t err = esp_littlefs_info(this->partition_label_.empty() ? this->mount_path_.c_str() : this->partition_label_.c_str(), &total_bytes, &used_bytes);
    if (err == ESP_OK) {
      ESP_LOGCONFIG(TAG, "  Total: %u bytes (%.1f KB)", total_bytes, total_bytes / 1024.0f);
      ESP_LOGCONFIG(TAG, "  Used: %u bytes (%.1f KB, %.1f%%)", used_bytes, used_bytes / 1024.0f,
                    (used_bytes * 100.0f) / total_bytes);
      ESP_LOGCONFIG(TAG, "  Free: %u bytes (%.1f KB)", total_bytes - used_bytes,
                    (total_bytes - used_bytes) / 1024.0f);
    }
  }
}

esp_vfs_littlefs_conf_t LittleFSMount::get_littlefs_config_() {
  esp_vfs_littlefs_conf_t conf = {};

  // Use partition label if specified, otherwise use mount path as base_path
  conf.base_path = this->mount_path_.c_str();
  conf.partition_label = this->partition_label_.empty() ? nullptr : this->partition_label_.c_str();
  conf.format_if_mount_failed = this->auto_format_;
  conf.dont_mount = false;

  // Configure based on device characteristics
  BlockDeviceConfig block_config = this->storage_->get_block_config();

  // LittleFS partition configuration (optional, can be auto-detected)
  // We'll let it auto-configure from the storage device

  return conf;
}

bool LittleFSMount::mount_() {
  ESP_LOGD(TAG, "Mounting LittleFS filesystem...");

  esp_vfs_littlefs_conf_t conf = this->get_littlefs_config_();

  esp_err_t err = esp_vfs_littlefs_register(&conf);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount LittleFS: %s (%d)", esp_err_to_name(err), err);

    if (err == ESP_FAIL && this->auto_format_) {
      ESP_LOGW(TAG, "Attempting to format filesystem...");
      if (this->format()) {
        // Retry mount after format
        err = esp_vfs_littlefs_register(&conf);
        if (err == ESP_OK) {
          ESP_LOGI(TAG, "Successfully mounted after format");
          this->mounted_ = true;
          return true;
        }
      }
    }

    return false;
  }

  this->mounted_ = true;
  return true;
}

bool LittleFSMount::unmount() {
  if (!this->mounted_) {
    return true;
  }

  ESP_LOGD(TAG, "Unmounting LittleFS from %s...", this->mount_path_.c_str());

  const char *partition = this->partition_label_.empty() ? this->mount_path_.c_str() : this->partition_label_.c_str();

  esp_err_t err = esp_vfs_littlefs_unregister(partition);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unmount: %s", esp_err_to_name(err));
    return false;
  }

  this->mounted_ = false;
  return true;
}

bool LittleFSMount::remount() {
  if (this->mounted_) {
    if (!this->unmount()) {
      return false;
    }
  }

  return this->mount_();
}

bool LittleFSMount::format() {
  ESP_LOGW(TAG, "Formatting LittleFS filesystem - ALL DATA WILL BE LOST!");

  const char *partition = this->partition_label_.empty() ? this->mount_path_.c_str() : this->partition_label_.c_str();

  // Unmount first if mounted
  bool was_mounted = this->mounted_;
  if (was_mounted) {
    this->unmount();
  }

  esp_err_t err = esp_littlefs_format(partition);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Format failed: %s", esp_err_to_name(err));
    return false;
  }

  ESP_LOGI(TAG, "Format successful");

  // Remount if it was mounted before
  if (was_mounted) {
    return this->mount_();
  }

  return true;
}

void LittleFSMount::register_with_storage_host_() {
#if defined(USE_STORAGE_HOST)
  // Check if storage_host is available via global accessor (soft dependency)
  if (storage_host::global_storage_host != nullptr) {
    // Storage host exists, register this mount point
    std::string platform = this->storage_->get_device_type();
    storage_host::global_storage_host->register_mount(this->mount_path_, platform);
    ESP_LOGI(TAG, "Registered LittleFS mount with storage_host: %s (platform: %s)", this->mount_path_.c_str(),
             platform.c_str());
  } else {
    ESP_LOGD(TAG, "storage_host not available, mount will be standalone");
  }
#else
  ESP_LOGD(TAG, "storage_host component not compiled, mount registration disabled");
#endif  // USE_STORAGE_HOST
}

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_ESP_IDF
