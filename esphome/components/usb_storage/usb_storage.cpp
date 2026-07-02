#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "usb_storage.h"
#include "usb_storage_diskio.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "usb/usb_helpers.h"
#include "usb/usb_types_ch9.h"

#include <cstring>
#include <cinttypes>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>

namespace esphome::usb_storage {

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
      this->mount_path_ = device->mount_path_;
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

void USBStorageClient::on_disconnected() {
  this->notify_disconnected_();
  this->disk_ready_ = false;

  if (this->mounted_) {
    char drive_path[8];
    snprintf(drive_path, sizeof(drive_path), "%d:", this->fatfs_drive_);
    f_mount(nullptr, drive_path, 0);
    esp_vfs_fat_unregister_path(this->mount_path_);
    this->mounted_ = false;
    ESP_LOGI(TAG, "FAT filesystem unmounted from '%s'", this->mount_path_);
  }

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
  ESP_LOGI(TAG, "Device unmounted");
  this->on_device_disconnected();
}

void USBStorageDevice::list_files() {
  ESP_LOGI(TAG, "Listing contents of '%s'", this->mount_path_);
  DIR *dh = opendir(this->mount_path_);
  if (!dh) {
    ESP_LOGE(TAG, "Failed to open directory: %s", this->mount_path_);
    return;
  }
  struct dirent *d;
  while ((d = readdir(dh)) != nullptr)
    ESP_LOGI(TAG, "  %s/%s", this->mount_path_, d->d_name);
  closedir(dh);
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

// ─────────────────────────────────────────────────────────────────────────────
// FilesystemStorage interface
// ─────────────────────────────────────────────────────────────────────────────

storage::StorageError USBStorageDevice::get_info(storage::StorageInfo *info) {
  info->id = this->mount_path_;
  info->name = "USB Storage";
  info->is_mounted = this->fs_mounted_;
  info->is_removable = true;
  info->is_read_only = false;
  info->block_size = this->parent_->get_sector_size();
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

storage::StorageError USBStorageDevice::unmount() {
  this->unmount_device();
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::format() {
  ESP_LOGW(TAG, "Format not implemented for USB storage");
  return storage::StorageError::NOT_READY;
}

storage::StorageError USBStorageDevice::sync() { return storage::StorageError::OK; }

storage::StorageError USBStorageDevice::open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;

  USBFileHandle *h = this->alloc_handle_();
  if (h == nullptr)
    return storage::StorageError::NO_SPACE;

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
    return storage::StorageError::NOT_FOUND;
  }

  handle = h;
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::close(storage::FileHandle *handle) {
  auto *h = static_cast<USBFileHandle *>(handle);
  if (h->file != nullptr)
    fclose(h->file);
  this->free_handle_(h);
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::read(storage::FileHandle *handle, uint8_t *buf, size_t len,
                                             size_t *bytes_transferred) {
  auto *h = static_cast<USBFileHandle *>(handle);
  size_t n = fread(buf, 1, len, h->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                                              size_t *bytes_transferred) {
  auto *h = static_cast<USBFileHandle *>(handle);
  size_t n = fwrite(buf, 1, len, h->file);
  if (bytes_transferred != nullptr)
    *bytes_transferred = n;
  return n == len ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError USBStorageDevice::seek(storage::FileHandle *handle, size_t offset) {
  auto *h = static_cast<USBFileHandle *>(handle);
  return fseek(h->file, static_cast<int32_t>(offset), SEEK_SET) == 0 ? storage::StorageError::OK
                                                                  : storage::StorageError::READ_ERROR;
}

storage::StorageError USBStorageDevice::tell(storage::FileHandle *handle, size_t *position) {
  auto *h = static_cast<USBFileHandle *>(handle);
  int32_t pos = static_cast<int32_t>(ftell(h->file));
  if (pos < 0)
    return storage::StorageError::READ_ERROR;
  *position = static_cast<size_t>(pos);
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::stat(const char *path, storage::FileStat *st) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(full, sizeof(full), path);
  struct stat s;
  if (::stat(full, &s) != 0)
    return storage::StorageError::NOT_FOUND;
  snprintf(st->name, sizeof(st->name), "%s", path);
  st->size = S_ISREG(s.st_mode) ? static_cast<size_t>(s.st_size) : 0;
  st->is_dir = S_ISDIR(s.st_mode);
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::list_dir(const char *path,
                                                 void (*callback)(const storage::FileStat *entry, void *ctx),
                                                 void *ctx) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(full, sizeof(full), path);
  DIR *dir = opendir(full);
  if (dir == nullptr)
    return storage::StorageError::NOT_FOUND;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    storage::FileStat st{};
    snprintf(st.name, sizeof(st.name), "%s", entry->d_name);
    st.is_dir = entry->d_type == DT_DIR;
    if (!st.is_dir) {
      char child_rel[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
      if (snprintf(child_rel, sizeof(child_rel), "%s/%s", path, entry->d_name) < static_cast<int>(sizeof(child_rel))) {
        char entry_full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
        this->build_path_(entry_full, sizeof(entry_full), child_rel);
        struct stat s;
        if (::stat(entry_full, &s) == 0)
          st.size = static_cast<size_t>(s.st_size);
      }
    }
    callback(&st, ctx);
  }
  closedir(dir);
  return storage::StorageError::OK;
}

storage::StorageError USBStorageDevice::mkdir(const char *path) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(full, sizeof(full), path);
  return ::mkdir(full, 0755) == 0 ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
}

storage::StorageError USBStorageDevice::rmdir(const char *path, bool recursive) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(full, sizeof(full), path);

  if (recursive) {
    DIR *dir = opendir(full);
    if (dir == nullptr)
      return storage::StorageError::NOT_FOUND;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      char child[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
      if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= static_cast<int>(sizeof(child)))
        continue;
      storage::StorageError err = (entry->d_type == DT_DIR) ? this->rmdir(child, true) : this->remove(child);
      if (err != storage::StorageError::OK) {
        closedir(dir);
        return err;
      }
    }
    closedir(dir);
  }
  return ::rmdir(full) == 0 ? storage::StorageError::OK : storage::StorageError::WRITE_ERROR;
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

storage::StorageError USBStorageDevice::copy(const char *src_path, const char *dst_path) {
  if (!this->fs_mounted_)
    return storage::StorageError::NOT_READY;
  char src_full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  char dst_full[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)];
  this->build_path_(src_full, sizeof(src_full), src_path);
  this->build_path_(dst_full, sizeof(dst_full), dst_path);

  FILE *src = fopen(src_full, "rb");
  if (src == nullptr)
    return storage::StorageError::NOT_FOUND;
  FILE *dst = fopen(dst_full, "wb");
  if (dst == nullptr) {
    fclose(src);
    return storage::StorageError::WRITE_ERROR;
  }
  uint8_t buf[256];
  size_t n;
  storage::StorageError err = storage::StorageError::OK;
  while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
    if (fwrite(buf, 1, n, dst) != n) {
      err = storage::StorageError::WRITE_ERROR;
      break;
    }
  }
  fclose(src);
  fclose(dst);
  return err;
}

}  // namespace esphome::usb_storage

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
