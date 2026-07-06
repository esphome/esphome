#pragma once

#include "esphome/components/storage/storage.h"

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "msc_bot.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <vector>

namespace esphome::usb_storage {

static constexpr const char *TAG = "usb_storage";

class USBStorageDevice;
class USBStorageClient;
template<typename... Ts> class ListFilesAction;

// ─────────────────────────────────────────────────────────────────────────────
// USBStorageClient
//
// Single USBClient subclass that owns the full MSC Bulk-Only Transport stack.
//
// Lifecycle:
//   setup()           — register FATFS DISKIO driver, create transfer semaphore
//   on_connected()    — parse bulk endpoints, run SCSI INQUIRY + READ CAPACITY,
//                       mount FAT filesystem, notify USBStorageDevice instances
//   on_disconnected() — unmount filesystem, reset drive slot
// ─────────────────────────────────────────────────────────────────────────────
class USBStorageClient : public usb_host::USBClient {
 public:
  USBStorageClient() : usb_host::USBClient(0, 0) { this->set_required_interface_class(USB_CLASS_MASS_STORAGE); }

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void add_device(USBStorageDevice *device) { this->devices_.push_back(device); }

  // Called from DISKIO driver (FATFS context) — blocking SCSI transfers
  bool scsi_read(uint32_t lba, uint8_t *buf, uint32_t count);
  bool scsi_write(uint32_t lba, const uint8_t *buf, uint32_t count);
  uint32_t get_sector_count() const { return this->sector_count_; }
  uint32_t get_sector_size() const { return this->sector_size_; }
  bool is_disk_ready() const { return this->disk_ready_; }

 protected:
  void on_connected() override;
  void on_disconnected() override;
  void on_removed(usb_device_handle_t handle) override;

  bool parse_msc_endpoints_();

  bool scsi_inquiry_();
  bool scsi_read_capacity_();
  bool scsi_test_unit_ready_();

  bool send_cbw_(uint32_t tag, uint32_t data_len, uint8_t flags, const uint8_t *cdb, uint8_t cdb_len);
  bool recv_data_(uint8_t *buf, uint16_t len);
  bool send_data_(const uint8_t *buf, uint16_t len);
  bool recv_csw_(uint32_t expected_tag);

  bool wait_transfer_(uint32_t timeout_ms = 5000);
  static void transfer_done_cb(const usb_host::TransferStatus &status, USBStorageClient *client);

  void notify_connected_(uint16_t vid, uint16_t pid);
  void notify_disconnected_();

  std::vector<USBStorageDevice *> devices_{};

  uint8_t bulk_in_ep_{0};
  uint8_t bulk_out_ep_{0};
  uint8_t msc_interface_{0xFF};

  uint32_t sector_count_{0};
  uint32_t sector_size_{512};
  uint32_t cbw_tag_{1};

  int fatfs_drive_{-1};
  bool disk_ready_{false};
  bool mounted_{false};
  const char *mount_path_{nullptr};

  SemaphoreHandle_t transfer_sem_{nullptr};
  bool transfer_ok_{false};
  uint16_t transfer_len_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// USBFileHandle — extends storage::FileHandle with no extra fields needed;
// the base FILE *file is sufficient since FAT is VFS-backed.
// ─────────────────────────────────────────────────────────────────────────────
struct USBFileHandle : public esphome::storage::FileHandle {
  char path_buf[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)]{};
};

// ─────────────────────────────────────────────────────────────────────────────
// USBStorageDevice
//
// User-facing device instance. Extends FilesystemStorage so it participates
// in the StorageRegistry. Mount/unmount are driven by USBStorageClient events;
// all file ops go through POSIX (FAT is already VFS-registered by the client).
// ─────────────────────────────────────────────────────────────────────────────
class USBStorageDevice : public esphome::storage::FilesystemStorage {
  friend class USBStorageClient;

 public:
  static constexpr int MAX_OPEN_FILES = 4;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_client(USBStorageClient *client) { this->client_ = client; }
  void set_mount_path(const char *mount_path) { this->mount_path_ = mount_path; }
  void set_vid(uint16_t vid) { this->vid_ = vid; }
  void set_pid(uint16_t pid) { this->pid_ = pid; }
  void set_storage_id(const char *id) { this->storage_id_ = id; }

  // Called by USBStorageClient on connect/disconnect events
  void on_device_connected(const char *mount_path);
  void on_device_disconnected();

  template<typename F> void add_on_mounted_callback(F &&cb) { this->on_mounted_.add(std::forward<F>(cb)); }
  const char *get_mount_path() const { return this->mount_path_; }
  bool is_mounted() const { return this->fs_mounted_; }

  bool remount_device();
  void unmount_device();

  // storage::FilesystemStorage interface
  storage::StorageError get_info(storage::StorageInfo *info) override;
  storage::StorageError mount() override;
  storage::StorageError unmount() override;
  storage::StorageError format() override;
  storage::StorageError sync() override;
  storage::StorageError open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) override;
  storage::StorageError close(storage::FileHandle *handle) override;
  storage::StorageError read(storage::FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred) override;
  storage::StorageError write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                              size_t *bytes_transferred) override;
  storage::StorageError seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) override;
  storage::StorageError tell(storage::FileHandle *handle, uint64_t *position) override;
  storage::StorageError stat(const char *path, storage::FileStat *stat) override;
  storage::StorageError list_dir(const char *path, bool (*callback)(const storage::FileStat *entry, void *ctx),
                                 void *ctx) override;
  storage::StorageError mkdir(const char *path) override;
  storage::StorageError rmdir(const char *path) override;
  storage::StorageError remove(const char *path) override;
  storage::StorageError rename(const char *old_path, const char *new_path) override;

  // USB stack transfers are self-contained (own endpoint transfers via a FreeRTOS semaphore,
  // no shared bus with other main-loop-driven components) — safe for the async worker to drive
  // from a background task, same as SdMmc's dedicated SDIO controller.
  uint8_t get_capabilities() const override { return storage::StorageCaps::STORAGE_CAP_IO_TASK_SAFE; }

 protected:
  template<typename... Ts> friend class ListFilesAction;

  USBFileHandle *alloc_handle_();
  void free_handle_(USBFileHandle *handle);
  void build_path_(char *out, size_t out_size, const char *path) const;

  void log_list_dir_start_(const char *path) const;
  // Matches the list_dir() callback signature (bool return = keep enumerating).
  static bool log_list_dir_entry(const storage::FileStat *entry, void *ctx);

  USBStorageClient *client_{nullptr};
  uint16_t vid_{0};
  uint16_t pid_{0};
  const char *mount_path_{nullptr};
  const char *storage_id_{nullptr};
  bool fs_mounted_{false};

  USBFileHandle handle_pool_[MAX_OPEN_FILES]{};
  LazyCallbackManager<void(const char *)> on_mounted_;
};

}  // namespace esphome::usb_storage
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
