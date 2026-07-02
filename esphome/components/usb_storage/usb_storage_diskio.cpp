#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "usb_storage_diskio.h"
#include "usb_storage.h"
#include "esphome/core/log.h"

namespace {
constexpr const char *TAG = "usb_diskio";
}  // namespace

namespace esphome::usb_storage {
static constexpr int MAX_DRIVES = FF_VOLUMES;

static USBStorageClient **get_clients() {
  static USBStorageClient *clients[MAX_DRIVES] = {};
  return clients;
}

// DISKIO callbacks — called from FATFS context (may be main loop or a dedicated task).
// All calls that do actual I/O block on a semaphore until the USB transfer callback fires.

static DSTATUS diskio_initialize(BYTE drive) {
  if (drive >= MAX_DRIVES || get_clients()[drive] == nullptr)
    return STA_NOINIT;
  return 0;  // RDY
}

static DSTATUS diskio_status(BYTE drive) {
  if (drive >= MAX_DRIVES || get_clients()[drive] == nullptr)
    return STA_NOINIT;
  return get_clients()[drive]->is_disk_ready() ? 0 : STA_NOINIT;
}

static DRESULT diskio_read(BYTE drive, BYTE *buf, LBA_t sector, UINT count) {
  if (drive >= MAX_DRIVES || get_clients()[drive] == nullptr)
    return RES_NOTRDY;
  return get_clients()[drive]->scsi_read(sector, buf, count) ? RES_OK : RES_ERROR;
}

static DRESULT diskio_write(BYTE drive, const BYTE *buf, LBA_t sector, UINT count) {
  if (drive >= MAX_DRIVES || get_clients()[drive] == nullptr)
    return RES_NOTRDY;
  return get_clients()[drive]->scsi_write(sector, buf, count) ? RES_OK : RES_ERROR;
}

static DRESULT diskio_ioctl(BYTE drive, BYTE cmd, void *buf) {
  if (drive >= MAX_DRIVES || get_clients()[drive] == nullptr)
    return RES_NOTRDY;

  USBStorageClient *client = get_clients()[drive];
  switch (cmd) {
    case CTRL_SYNC:
      return RES_OK;
    case GET_SECTOR_COUNT:
      *reinterpret_cast<LBA_t *>(buf) = client->get_sector_count();
      return RES_OK;
    case GET_SECTOR_SIZE:
      *reinterpret_cast<WORD *>(buf) = static_cast<WORD>(client->get_sector_size());
      return RES_OK;
    case GET_BLOCK_SIZE:
      *reinterpret_cast<DWORD *>(buf) = 1;  // erase block size in sectors — 1 = unknown
      return RES_OK;
    default:
      return RES_PARERR;
  }
}

static const ff_diskio_impl_t USB_DISKIO_DRIVER = {
    .init = diskio_initialize,
    .status = diskio_status,
    .read = diskio_read,
    .write = diskio_write,
    .ioctl = diskio_ioctl,
};

int usb_diskio_register(USBStorageClient *client) {
  for (int i = 0; i < MAX_DRIVES; i++) {
    if (get_clients()[i] == nullptr) {
      get_clients()[i] = client;
      ff_diskio_register(static_cast<BYTE>(i), &USB_DISKIO_DRIVER);
      ESP_LOGD(TAG, "Registered USB DISKIO on drive %d", i);
      return i;
    }
  }
  ESP_LOGE(TAG, "No free FATFS drive slots (max %d)", MAX_DRIVES);
  return -1;
}

void usb_diskio_unregister(int drive) {
  if (drive < 0 || drive >= MAX_DRIVES)
    return;
  get_clients()[drive] = nullptr;
  ff_diskio_register(static_cast<BYTE>(drive), nullptr);
  ESP_LOGD(TAG, "Unregistered USB DISKIO on drive %d", drive);
}

}  // namespace esphome::usb_storage

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
