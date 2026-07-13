#include "esphome/components/storage/storage.h"
#ifdef USE_STORAGE_WORKER
#include "esphome/components/storage/storage_worker.h"
#endif

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "usb_storage.h"
#include "usb_storage_diskio.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "usb/usb_helpers.h"
#include "usb/usb_types_ch9.h"

#include <cstring>
#include <cinttypes>
#include <cerrno>

namespace esphome::usb_storage {

namespace {

// Maps a FATFS FRESULT to the closest StorageError. `for_rmdir` selects FR_DENIED's mapping:
// f_unlink() (used for rmdir — see rmdir() below, FATFS has no dedicated f_rmdir) returns
// FR_DENIED both for "directory not empty" and for genuine permission/read-only failures, so the
// caller must tell us which context applies.
storage::StorageError fresult_to_storage_error(FRESULT res, bool for_rmdir, bool is_write) {
  switch (res) {
    case FR_OK:
      return storage::StorageError::OK;
    case FR_NO_FILE:
    case FR_NO_PATH:
      return storage::StorageError::NOT_FOUND;
    case FR_EXIST:
      return storage::StorageError::ALREADY_EXISTS;
    case FR_DENIED:
      return for_rmdir ? storage::StorageError::NOT_EMPTY : storage::StorageError::PERMISSION_DENIED;
    case FR_INVALID_NAME:
      return storage::StorageError::INVALID_ARGS;
    case FR_NOT_READY:
      return storage::StorageError::NOT_READY;
    case FR_WRITE_PROTECTED:
      return storage::StorageError::PERMISSION_DENIED;
    default:
      return is_write ? storage::StorageError::WRITE_ERROR : storage::StorageError::READ_ERROR;
  }
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Transfer semaphore helpers
// ─────────────────────────────────────────────────────────────────────────────

void USBStorageClient::transfer_done_cb(const usb_host::TransferStatus &status, USBStorageClient *client) {
  client->transfer_ok_ = status.success;
  client->transfer_len_ = static_cast<uint16_t>(status.data_len);
  xSemaphoreGive(client->transfer_sem_);
}

bool USBStorageClient::wait_transfer_(uint32_t timeout_ms) {
  return xSemaphoreTake(this->transfer_sem_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE && this->transfer_ok_;
}

// ─────────────────────────────────────────────────────────────────────────────
// USBStorageClient — setup / loop
// ─────────────────────────────────────────────────────────────────────────────

void USBStorageClient::setup() {
  this->transfer_sem_ = xSemaphoreCreateBinary();
  if (this->transfer_sem_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create transfer semaphore");
    this->mark_failed();
    return;
  }

  this->fatfs_drive_ = usb_diskio_register(this);
  if (this->fatfs_drive_ < 0) {
    ESP_LOGE(TAG, "Failed to register FATFS DISKIO drive");
    this->mark_failed();
    return;
  }

  USBClient::setup();
}

void USBStorageClient::loop() { USBClient::loop(); }

// ─────────────────────────────────────────────────────────────────────────────
// Endpoint parsing
// ─────────────────────────────────────────────────────────────────────────────

bool USBStorageClient::parse_msc_endpoints_() {
  const usb_config_desc_t *cfg = this->get_config_desc_();
  if (cfg == nullptr)
    return false;

  this->bulk_in_ep_ = 0;
  this->bulk_out_ep_ = 0;

  int offset = 0;
  const usb_standard_desc_t *next = reinterpret_cast<const usb_standard_desc_t *>(cfg);
  bool in_msc_intf = false;

  while ((next = usb_parse_next_descriptor(next, cfg->wTotalLength, &offset)) != nullptr) {
    if (next->bDescriptorType == USB_W_VALUE_DT_INTERFACE) {
      const auto *intf = reinterpret_cast<const usb_intf_desc_t *>(next);
      in_msc_intf = (intf->bInterfaceClass == USB_CLASS_MASS_STORAGE);
      if (in_msc_intf)
        this->msc_interface_ = intf->bInterfaceNumber;
    } else if (in_msc_intf && next->bDescriptorType == USB_W_VALUE_DT_ENDPOINT) {
      const auto *ep = reinterpret_cast<const usb_ep_desc_t *>(next);
      if ((ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_BULK)
        continue;
      if (USB_EP_DESC_GET_EP_DIR(ep)) {
        this->bulk_in_ep_ = ep->bEndpointAddress;
      } else {
        this->bulk_out_ep_ = ep->bEndpointAddress;
      }
    }
    if (this->bulk_in_ep_ && this->bulk_out_ep_)
      break;
  }

  if (!this->bulk_in_ep_ || !this->bulk_out_ep_) {
    ESP_LOGE(TAG, "Failed to find MSC bulk endpoints (in=0x%02X out=0x%02X)", this->bulk_in_ep_, this->bulk_out_ep_);
    return false;
  }
  ESP_LOGD(TAG, "MSC endpoints: bulk_in=0x%02X bulk_out=0x%02X intf=%d", this->bulk_in_ep_, this->bulk_out_ep_,
           this->msc_interface_);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// BOT primitives
// ─────────────────────────────────────────────────────────────────────────────

bool USBStorageClient::send_cbw_(uint32_t tag, uint32_t data_len, uint8_t flags, const uint8_t *cdb, uint8_t cdb_len) {
  MscCbw cbw{};
  cbw.tag = tag;
  cbw.data_transfer_length = data_len;
  cbw.flags = flags;
  cbw.lun = 0;
  cbw.cb_length = cdb_len;
  memcpy(cbw.cb, cdb, cdb_len);

  auto cb = [this](const usb_host::TransferStatus &s) { transfer_done_cb(s, this); };
  if (!this->transfer_out(this->bulk_out_ep_, cb, reinterpret_cast<const uint8_t *>(&cbw), sizeof(MscCbw)))
    return false;
  return this->wait_transfer_();
}

bool USBStorageClient::recv_data_(uint8_t *buf, uint16_t len) {
  auto cb = [this](const usb_host::TransferStatus &s) { transfer_done_cb(s, this); };
  if (!this->transfer_in(this->bulk_in_ep_, cb, len))
    return false;
  return this->wait_transfer_();
}

bool USBStorageClient::send_data_(const uint8_t *buf, uint16_t len) {
  auto cb = [this](const usb_host::TransferStatus &s) { transfer_done_cb(s, this); };
  return this->transfer_out(this->bulk_out_ep_, cb, buf, len) && this->wait_transfer_();
}

bool USBStorageClient::recv_csw_(uint32_t expected_tag) {
  MscCsw csw{};
  auto cb = [this, &csw](const usb_host::TransferStatus &s) {
    if (s.success && s.data_len >= sizeof(MscCsw))
      memcpy(&csw, s.data, sizeof(MscCsw));
    transfer_done_cb(s, this);
  };
  if (!this->transfer_in(this->bulk_in_ep_, cb, sizeof(MscCsw)))
    return false;
  if (!this->wait_transfer_())
    return false;
  if (csw.signature != MSC_BOT_CSW_SIGNATURE) {
    ESP_LOGE(TAG, "Invalid CSW signature: 0x%08" PRIX32, csw.signature);
    return false;
  }
  if (csw.tag != expected_tag) {
    ESP_LOGE(TAG, "CSW tag mismatch: expected %" PRIu32 " got %" PRIu32, expected_tag, csw.tag);
    return false;
  }
  if (csw.status != MSC_BOT_CSW_STATUS_GOOD) {
    ESP_LOGW(TAG, "CSW status: %d", csw.status);
    return false;
  }
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SCSI commands
// ─────────────────────────────────────────────────────────────────────────────

bool USBStorageClient::scsi_test_unit_ready_() {
  uint8_t cdb[6] = {SCSI_CMD_TEST_UNIT_READY, 0, 0, 0, 0, 0};
  uint32_t tag = this->cbw_tag_++;
  return this->send_cbw_(tag, 0, MSC_BOT_CBW_FLAGS_OUT, cdb, sizeof(cdb)) && this->recv_csw_(tag);
}

bool USBStorageClient::scsi_inquiry_() {
  uint8_t cdb[6] = {SCSI_CMD_INQUIRY, 0, 0, 0, sizeof(ScsiInquiryResponse), 0};
  uint32_t tag = this->cbw_tag_++;
  ScsiInquiryResponse resp{};

  auto cb = [this, &resp](const usb_host::TransferStatus &s) {
    if (s.success && s.data_len >= sizeof(ScsiInquiryResponse))
      memcpy(&resp, s.data, sizeof(ScsiInquiryResponse));
    transfer_done_cb(s, this);
  };

  if (!this->send_cbw_(tag, sizeof(ScsiInquiryResponse), MSC_BOT_CBW_FLAGS_IN, cdb, sizeof(cdb)))
    return false;
  if (!this->transfer_in(this->bulk_in_ep_, cb, sizeof(ScsiInquiryResponse)))
    return false;
  if (!this->wait_transfer_())
    return false;
  if (!this->recv_csw_(tag))
    return false;

  char vendor[9]{}, product[17]{};
  memcpy(vendor, resp.vendor_id, 8);
  memcpy(product, resp.product_id, 16);
  ESP_LOGI(TAG, "SCSI INQUIRY: vendor='%s' product='%s' type=0x%02X", vendor, product,
           resp.peripheral_qualifier_type & 0x1F);
  return (resp.peripheral_qualifier_type & 0x1F) == 0x00;
}

bool USBStorageClient::scsi_read_capacity_() {
  uint8_t cdb[10] = {SCSI_CMD_READ_CAPACITY_10, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t tag = this->cbw_tag_++;
  ScsiReadCapacity10Response resp{};

  auto cb = [this, &resp](const usb_host::TransferStatus &s) {
    if (s.success && s.data_len >= sizeof(ScsiReadCapacity10Response))
      memcpy(&resp, s.data, sizeof(ScsiReadCapacity10Response));
    transfer_done_cb(s, this);
  };

  if (!this->send_cbw_(tag, sizeof(ScsiReadCapacity10Response), MSC_BOT_CBW_FLAGS_IN, cdb, sizeof(cdb)))
    return false;
  if (!this->transfer_in(this->bulk_in_ep_, cb, sizeof(ScsiReadCapacity10Response)))
    return false;
  if (!this->wait_transfer_())
    return false;
  if (!this->recv_csw_(tag))
    return false;

  this->sector_count_ = __builtin_bswap32(resp.last_lba) + 1;
  this->sector_size_ = __builtin_bswap32(resp.block_size);
  ESP_LOGI(TAG, "SCSI READ CAPACITY: sectors=%" PRIu32 " sector_size=%" PRIu32 " (%.1f MB)", this->sector_count_,
           this->sector_size_, static_cast<float>(this->sector_count_) * this->sector_size_ / (1024.0f * 1024.0f));
  return this->sector_size_ > 0 && this->sector_count_ > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public DISKIO interface (called from FATFS context — blocking)
// ─────────────────────────────────────────────────────────────────────────────

bool USBStorageClient::scsi_read(uint32_t lba, uint8_t *buf, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    uint8_t cdb[10] = {
        SCSI_CMD_READ_10,
        0,
        static_cast<uint8_t>((lba + i) >> 24),
        static_cast<uint8_t>((lba + i) >> 16),
        static_cast<uint8_t>((lba + i) >> 8),
        static_cast<uint8_t>((lba + i) & 0xFF),
        0,
        0,
        1,
        0,
    };
    uint32_t tag = this->cbw_tag_++;
    uint8_t *sector_buf = buf + i * this->sector_size_;

    auto cb = [this, sector_buf, sz = this->sector_size_](const usb_host::TransferStatus &s) {
      if (s.success && s.data_len >= sz)
        memcpy(sector_buf, s.data, sz);
      transfer_done_cb(s, this);
    };

    if (!this->send_cbw_(tag, this->sector_size_, MSC_BOT_CBW_FLAGS_IN, cdb, sizeof(cdb)))
      return false;
    if (!this->transfer_in(this->bulk_in_ep_, cb, static_cast<uint16_t>(this->sector_size_)))
      return false;
    if (!this->wait_transfer_())
      return false;
    if (!this->recv_csw_(tag))
      return false;
  }
  return true;
}

bool USBStorageClient::scsi_write(uint32_t lba, const uint8_t *buf, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    uint8_t cdb[10] = {
        SCSI_CMD_WRITE_10,
        0,
        static_cast<uint8_t>((lba + i) >> 24),
        static_cast<uint8_t>((lba + i) >> 16),
        static_cast<uint8_t>((lba + i) >> 8),
        static_cast<uint8_t>((lba + i) & 0xFF),
        0,
        0,
        1,
        0,
    };
    uint32_t tag = this->cbw_tag_++;
    const uint8_t *sector_buf = buf + i * this->sector_size_;

    if (!this->send_cbw_(tag, this->sector_size_, MSC_BOT_CBW_FLAGS_OUT, cdb, sizeof(cdb)))
      return false;
    if (!this->send_data_(sector_buf, static_cast<uint16_t>(this->sector_size_)))
      return false;
    if (!this->recv_csw_(tag))
      return false;
  }
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Connect / disconnect
// ─────────────────────────────────────────────────────────────────────────────

void USBStorageClient::notify_connected_(uint16_t vid, uint16_t pid) {
  for (auto *device : this->devices_) {
    if ((device->vid_ == 0 && device->pid_ == 0) || (device->vid_ == vid && device->pid_ == pid)) {
      if (!device->is_mounted()) {
        device->on_device_connected(this->mount_path_);
        return;
      }
    }
  }
  ESP_LOGW(TAG, "No storage device configured for VID=0x%04X PID=0x%04X", vid, pid);
}

void USBStorageClient::notify_disconnected_() {
  for (auto *device : this->devices_) {
    if (device->is_mounted())
      device->on_device_disconnected();
  }
}

void USBStorageClient::on_connected() {
  const usb_device_desc_t *dev = this->get_device_desc_();
  uint16_t vid = dev ? dev->idVendor : 0;
  uint16_t pid = dev ? dev->idProduct : 0;

  ESP_LOGI(TAG, "MSC device connected VID=0x%04X PID=0x%04X", vid, pid);

  if (!this->parse_msc_endpoints_()) {
    this->disconnect();
    return;
  }

  if (!this->claim_interface(this->msc_interface_, 0)) {
    ESP_LOGE(TAG, "Failed to claim MSC interface %d", this->msc_interface_);
    this->disconnect();
    return;
  }

  if (!this->scsi_inquiry_()) {
    ESP_LOGE(TAG, "SCSI INQUIRY failed");
    this->disconnect();
    return;
  }

  if (!this->scsi_read_capacity_()) {
    ESP_LOGE(TAG, "SCSI READ CAPACITY failed");
    this->disconnect();
    return;
  }

  this->disk_ready_ = true;

  // Pick mount path from first matching device, fall back to /usb
  this->mount_path_ = "/usb";
  for (auto *device : this->devices_) {
    if ((device->vid_ == 0 && device->pid_ == 0) || (device->vid_ == vid && device->pid_ == pid)) {
      this->mount_path_ = device->get_mount_path();
      break;
    }
  }

  char drive_path[8];
  snprintf(drive_path, sizeof(drive_path), "%d:", this->fatfs_drive_);

  FATFS *fs = nullptr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  esp_vfs_fat_conf_t vfs_conf = {
      .base_path = this->mount_path_,
      .fat_drive = drive_path,
      .max_files = 5,
  };
  esp_err_t err = esp_vfs_fat_register_cfg(&vfs_conf, &fs);
#else
  esp_err_t err = esp_vfs_fat_register(this->mount_path_, drive_path, 5, &fs);
#endif
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_vfs_fat_register failed: %s", esp_err_to_name(err));
    this->disk_ready_ = false;
    this->disconnect();
    return;
  }

  FRESULT res = f_mount(fs, drive_path, 1);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "f_mount failed: %d", res);
    esp_vfs_fat_unregister_path(this->mount_path_);
    this->disk_ready_ = false;
    this->disconnect();
    return;
  }

  this->mounted_ = true;
  ESP_LOGI(TAG, "FAT filesystem mounted at '%s'", this->mount_path_);

  this->notify_connected_(vid, pid);
}

void USBStorageClient::unmount_filesystem() {
  if (!this->mounted_)
    return;
  char drive_path[8];
  snprintf(drive_path, sizeof(drive_path), "%d:", this->fatfs_drive_);
  f_mount(nullptr, drive_path, 0);
  esp_vfs_fat_unregister_path(this->mount_path_);
  this->mounted_ = false;
  ESP_LOGI(TAG, "FAT filesystem unmounted from '%s'", this->mount_path_);
}

void USBStorageClient::on_disconnected() {
  this->notify_disconnected_();
  this->disk_ready_ = false;

  this->unmount_filesystem();

  if (this->msc_interface_ != 0xFF) {
    this->release_interface(this->msc_interface_);
    this->msc_interface_ = 0xFF;
  }

  this->sector_count_ = 0;
  this->bulk_in_ep_ = 0;
  this->bulk_out_ep_ = 0;
}

void USBStorageClient::on_removed(usb_device_handle_t handle) { USBClient::on_removed(handle); }

// ─────────────────────────────────────────────────────────────────────────────
// USBStorageDevice — lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void USBStorageDevice::setup() { ESP_LOGCONFIG(TAG, "Setting up USB Storage Device"); }

void USBStorageDevice::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Storage Device:");
  ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_ ? this->mount_path_ : "(none)");
  ESP_LOGCONFIG(TAG, "  VID: 0x%04X  PID: 0x%04X", this->vid_, this->pid_);
}

void USBStorageDevice::on_device_connected(const char *mount_path) {
  ESP_LOGI(TAG, "USB Storage Device connected (mount_path='%s')", mount_path);
  this->mount_path_ = mount_path;
  this->fs_mounted_ = true;
  snprintf(this->fatfs_drive_, sizeof(this->fatfs_drive_), "%d:", this->client_->get_fatfs_drive());

  this->on_mounted_.call(mount_path);

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->register_storage(this);
}

void USBStorageDevice::on_device_disconnected() {
  ESP_LOGI(TAG, "USB Storage Device disconnected (mount_path='%s')", this->mount_path_ ? this->mount_path_ : "(none)");
  this->fs_mounted_ = false;

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->unregister_storage(this);
}

bool USBStorageDevice::remount_device() {
  if (!this->fs_mounted_) {
    ESP_LOGW(TAG, "Device not mounted, cannot remount");
    return false;
  }
  ESP_LOGI(TAG, "Device remounted successfully");
  this->on_mounted_.call(this->mount_path_);
  return true;
}

void USBStorageDevice::unmount_device() {
  if (!this->fs_mounted_)
    return;
  // Safe-eject semantics: unregister from the storage registry first (drains worker jobs,
  // see the registry contract), then tear the FAT mount + VFS registration down completely.
  // The device intentionally "disappears" — get_mount_caps() is UNMOUNT-only, so there is no
  // mount button; the next mount happens when the stick is physically (re)inserted, which
  // re-runs the client's connect flow.
  ESP_LOGI(TAG, "Device unmounted");
  this->on_device_disconnected();
  if (this->client_ != nullptr)
    this->client_->unmount_filesystem();
}

void USBStorageDevice::log_list_dir_start_(const char *path) const { ESP_LOGI(TAG, "Listing files in: %s", path); }

bool USBStorageDevice::log_list_dir_entry(const storage::FileStat *entry, void *ctx) {
  if (entry->is_dir) {
    ESP_LOGI(TAG, "  [DIR]  %s", entry->name);
  } else {
    ESP_LOGI(TAG, "  [FILE] %s (%llu bytes)", entry->name, static_cast<unsigned long long>(entry->size));
  }
  return true;  // keep enumerating — this is a "list everything" action
}

// ─────────────────────────────────────────────────────────────────────────────
// Handle pool helpers
// ─────────────────────────────────────────────────────────────────────────────

USBFileHandle *USBStorageDevice::alloc_handle_() {
  for (auto &h : this->handle_pool_) {
    if (!h.in_use) {
      h.in_use = true;
      h.storage = this;
      h.file = nullptr;
      return &h;
    }
  }
  return nullptr;
}

void USBStorageDevice::free_handle_(USBFileHandle *handle) {
  handle->in_use = false;
  handle->file = nullptr;
  handle->path = nullptr;
}

void USBStorageDevice::build_path_(char *out, size_t out_size, const char *path) const {
  if (path[0] == '/') {
    snprintf(out, out_size, "%s%s", this->mount_path_, path);
  } else {
    snprintf(out, out_size, "%s/%s", this->mount_path_, path);
  }
}

bool USBStorageDevice::build_fatfs_path_(const char *rel_path, char *out, size_t out_size) const {
  int written;
  if (rel_path[0] == '/') {
    written = snprintf(out, out_size, "%s%s", this->fatfs_drive_, rel_path);
  } else {
    written = snprintf(out, out_size, "%s/%s", this->fatfs_drive_, rel_path);
  }
  return written > 0 && static_cast<size_t>(written) < out_size;
}

// ─────────────────────────────────────────────────────────────────────────────
// FilesystemStorage interface
// ─────────────────────────────────────────────────────────────────────────────

storage::StorageError USBStorageDevice::get_info(storage::StorageInfo *info) {
  info->id = this->mount_path_;
  info->name = "USB Storage";
  info->is_mounted = this->fs_mounted_;
  info->is_removable = true;
  info->is_read_only = false;
  info->block_size = this->client_->get_sector_size();
  info->total_bytes = 0;
  info->free_bytes = 0;

  if (this->fs_mounted_) {
    FATFS *fs;
    DWORD fre_clust;
    char path[8];
    snprintf(path, sizeof(path), "%s/", this->mount_path_);
    if (f_getfree(path, &fre_clust, &fs) == FR_OK) {
      DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
      DWORD fre_sect = fre_clust * fs->csize;
      info->total_bytes = static_cast<uint64_t>(tot_sect) * fs->ssize;
      info->free_bytes = static_cast<uint64_t>(fre_sect) * fs->ssize;
    }
  }
  return storage::StorageError::OK;
}

// mount/unmount are driven by USBStorageClient events — these are no-ops
// when called directly (the client owns the FAT mount lifecycle).
storage::StorageError USBStorageDevice::mount() {
  return this->fs_mounted_ ? storage::StorageError::OK : storage::StorageError::NOT_READY;
}

bool USBStorageDevice::has_open_handles_() const {
  for (const auto &h : this->handle_pool_) {
    if (h.in_use)
      return true;
  }
  return false;
}

storage::StorageError USBStorageDevice::unmount() {
  // Already unmounted -> target state reached, no error (see design notes).
  if (!this->fs_mounted_)
    return storage::StorageError::OK;
  // An unmount is already being carried out asynchronously.
  if (this->unmount_pending_)
    return storage::StorageError::NOT_READY;
  // Request the unmount; loop() completes it once the device is quiescent (no open handle,
  // no worker job referencing this device). Typical USB "safe eject" behaviour: in-flight
  // transfers finish, then we sync and unmount. Returns OK = unmount accepted/initiated.
  this->unmount_pending_ = true;
  ESP_LOGI(TAG, "Unmount requested; will complete once active transfers finish");
  return storage::StorageError::OK;
}

void USBStorageDevice::loop() {
  if (!this->unmount_pending_)
    return;
  // Wait until nothing references the device: no open file handle, and no worker job whose
  // src/dst is this device. The worker keeps a handle open for a transfer's whole duration,
  // so has_open_handles_() already covers active copies; the extra registry check guards the
  // brief open()/close() edges where a job exists but its handle is momentarily not counted.
  if (this->has_open_handles_())
    return;
#ifdef USE_STORAGE_WORKER
  if (storage::global_storage_worker != nullptr && storage::global_storage_worker->is_busy_with(this))
    return;
#endif
  // Quiescent — sync then physically unmount.
  this->sync();
  this->unmount_pending_ = false;
  this->unmount_device();
}

storage::StorageError USBStorageDevice::format() {
  ESP_LOGW(TAG, "Format not implemented for USB storage");
  return storage::StorageError::NOT_SUPPORTED;
}

storage::StorageError USBStorageDevice::sync() {
  // Flush any open handle's buffered data to the medium (same as SdStorageBase::sync()).
  // handle->file is a POSIX FILE* on the FAT VFS mount. In the deferred-unmount path there
  // are usually no open handles left (unmount waits for that), but sync() is also callable
  // on its own, and flushing here still commits FATFS-internal buffers on the next write.
  storage::StorageError err = storage::StorageError::OK;
  for (auto &h : this->handle_pool_) {
    if (!h.in_use || h.file == nullptr)
      continue;
    if (fflush(h.file) != 0 || fsync(fileno(h.file)) != 0)
      err = storage::StorageError::WRITE_ERROR;
  }
  return err;
}

storage::StorageError USBStorageDevice::open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;

  USBFileHandle *h = this->alloc_handle_();
  if (h == nullptr)
    return storage::StorageError::TOO_MANY_OPEN_FILES;

  this->build_path_(h->path_buf, sizeof(h->path_buf), path);
  h->path = h->path_buf;

  const char *fmode = "rb";
  switch (mode) {
    case storage::OpenMode::WRITE:
      fmode = "wb";
      break;
    case storage::OpenMode::APPEND:
      fmode = "ab";
      break;
    case storage::OpenMode::READ_WRITE:
      fmode = "r+b";
      break;
    default:
      break;
  }

  h->file = fopen(h->path_buf, fmode);
  if (h->file == nullptr) {
    this->free_handle_(h);
    switch (errno) {
      case ENOENT:
        return storage::StorageError::NOT_FOUND;
      case ENOSPC:
        return storage::StorageError::NO_SPACE;
      case EACCES:
      case EROFS:
        return storage::StorageError::PERMISSION_DENIED;
      case EMFILE:
      case ENFILE:
        return storage::StorageError::TOO_MANY_OPEN_FILES;
      default:
        return mode == storage::OpenMode::READ ? storage::StorageError::READ_ERROR : storage::StorageError::WRITE_ERROR;
    }
  }

  handle = h;
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::close(storage::FileHandle *handle) {
  if (handle == nullptr || !handle->in_use)
    return storage::StorageError::INVALID_ARGS;
  auto *h = static_cast<USBFileHandle *>(handle);
  storage::StorageError err = storage::StorageError::OK;
  if (h->file != nullptr) {
    if (fclose(h->file) != 0)
      err = storage::StorageError::WRITE_ERROR;
  }
  this->free_handle_(h);
  return err;
}

storage::StorageError USBStorageDevice::read(storage::FileHandle *handle, uint8_t *buf, size_t len,
                                             size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  auto *h = static_cast<USBFileHandle *>(handle);
  size_t n = fread(buf, 1, len, h->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  // fread() returning less than requested means either EOF (not an error, per the
  // partial-read contract in storage.h) or a real I/O error — ferror() disambiguates.
  if (n < len && ferror(h->file)) {
    clearerr(h->file);
    return storage::StorageError::READ_ERROR;
  }
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                                              size_t *bytes_transferred) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  auto *h = static_cast<USBFileHandle *>(handle);
  size_t n = fwrite(buf, 1, len, h->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  return n == len ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError USBStorageDevice::seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  // ESP-IDF's newlib fseek() takes a 32-bit `long` offset — FATFS/POSIX on this platform can't
  // address beyond that anyway (files >4GB aren't representable here), so reject rather than
  // silently truncating a caller-supplied 64-bit offset (the interface allows >4GB elsewhere,
  // e.g. NetworkStorage).
  if (offset > INT32_MAX || offset < INT32_MIN)
    return storage::StorageError::INVALID_ARGS;
  int whence;
  switch (mode) {
    case storage::SeekMode::SET:
      whence = SEEK_SET;
      break;
    case storage::SeekMode::CUR:
      whence = SEEK_CUR;
      break;
    case storage::SeekMode::END:
      whence = SEEK_END;
      break;
    default:
      return storage::StorageError::INVALID_ARGS;
  }
  auto *h = static_cast<USBFileHandle *>(handle);
  return fseek(h->file, static_cast<int32_t>(offset), whence) == 0 ? storage::StorageError::OK
                                                                   : storage::StorageError::READ_ERROR;
}

storage::StorageError USBStorageDevice::tell(storage::FileHandle *handle, uint64_t *position) {
  if (handle == nullptr || !handle->in_use || handle->file == nullptr)
    return storage::StorageError::INVALID_ARGS;
  auto *h = static_cast<USBFileHandle *>(handle);
  int32_t pos = static_cast<int32_t>(ftell(h->file));
  if (pos < 0)
    return storage::StorageError::READ_ERROR;
  *position = static_cast<uint64_t>(pos);
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::stat(const char *path, storage::FileStat *st) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;

  // f_stat() on the drive root ("N:/") fails by FATFS design — synthesize the result instead of
  // calling it. path is "" or "/" exactly when the caller asked to stat the mount point itself.
  if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
    st->name[0] = '\0';
    st->size = 0;
    st->is_dir = true;
    st->mtime = 0;
    return storage::StorageError::OK;
  }

  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  FILINFO fno;
  FRESULT res = f_stat(full, &fno);
  if (res != FR_OK)
    return fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/false);

  // FileStat::name is the basename only (see the contract on the struct in storage.h) —
  // consistent with what list_dir() puts there, regardless of how many path segments the
  // caller passed in.
  StringRef path_ref(path);
  size_t base_pos = 0;
  for (size_t i = path_ref.size(); i > 0; i--) {
    if (path_ref[i - 1] == '/') {
      base_pos = i;
      break;
    }
  }
  StringRef base_ref(path + base_pos, path_ref.size() - base_pos);
  size_t name_len = base_ref.copy(st->name, sizeof(st->name) - 1);
  st->name[name_len] = '\0';
  st->is_dir = (fno.fattrib & AM_DIR) != 0;
  st->size = st->is_dir ? 0 : static_cast<uint64_t>(fno.fsize);
  st->mtime = 0;  // FatFs FILINFO exposes fdate/ftime (DOS format), not a Unix timestamp.
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::list_dir(const char *path,
                                                 bool (*callback)(const storage::FileStat *entry, void *ctx),
                                                 void *ctx) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  FF_DIR fat_dir;
  FRESULT res = f_opendir(&fat_dir, full);
  if (res != FR_OK)
    return fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/false);

  FILINFO fno;
  for (;;) {
    if (f_readdir(&fat_dir, &fno) != FR_OK || fno.fname[0] == '\0')
      break;
    StringRef fname_ref(fno.fname);
    if (fname_ref == "." || fname_ref == "..")
      continue;

    storage::FileStat st{};
    size_t name_len = fname_ref.copy(st.name, sizeof(st.name) - 1);
    st.name[name_len] = '\0';
    st.is_dir = (fno.fattrib & AM_DIR) != 0;
    st.size = st.is_dir ? 0 : static_cast<uint64_t>(fno.fsize);
    st.mtime = 0;  // FatFs FILINFO exposes fdate/ftime (DOS format), not a Unix timestamp.

    // callback returns false to stop enumeration early — that is not an error, so we still
    // return OK below regardless of how the loop exits.
    if (!callback(&st, ctx))
      break;
  }
  f_closedir(&fat_dir);
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::mkdir(const char *path) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  FRESULT res = f_mkdir(full);
  return fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/true);
}

storage::StorageError USBStorageDevice::rmdir(const char *path) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  if (!this->build_fatfs_path_(path, full, sizeof(full)))
    return storage::StorageError::INVALID_ARGS;

  // Non-recursive per the storage:: contract: must fail with NOT_EMPTY if the directory has
  // contents. Recursive delete is the free storage::remove_recursive() helper, built on top
  // of list_dir()/remove()/this rmdir() — no need to duplicate that tree-walk here.
  FF_DIR fat_dir;
  FRESULT res = f_opendir(&fat_dir, full);
  if (res != FR_OK)
    return fresult_to_storage_error(res, /*for_rmdir=*/true, /*is_write=*/false);

  bool has_entries = false;
  FILINFO fno;
  while (f_readdir(&fat_dir, &fno) == FR_OK && fno.fname[0] != '\0') {
    StringRef fname_ref(fno.fname);
    if (fname_ref == "." || fname_ref == "..")
      continue;
    has_entries = true;
    break;
  }
  f_closedir(&fat_dir);
  if (has_entries)
    return storage::StorageError::NOT_EMPTY;

  // FATFS removes empty directories via f_unlink() — there is no dedicated f_rmdir().
  res = f_unlink(full);
  return fresult_to_storage_error(res, /*for_rmdir=*/true, /*is_write=*/true);
}

storage::StorageError USBStorageDevice::remove(const char *path) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(full, sizeof(full), path);
  return ::remove(full) == 0 ? storage::StorageError::OK : storage::StorageError::NOT_FOUND;
}

storage::StorageError USBStorageDevice::rename(const char *old_path, const char *new_path) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char old_full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  char new_full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(old_full, sizeof(old_full), old_path);
  this->build_path_(new_full, sizeof(new_full), new_path);
  return ::rename(old_full, new_full) == 0 ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

}  // namespace esphome::usb_storage

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
