#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "ff.h"
#include "diskio.h"
#include "diskio_impl.h"

namespace esphome::usb_storage {

class USBStorageClient;

// Register/unregister a USBStorageClient as a FATFS DISKIO drive.
// Returns the assigned drive number (0-based), or -1 on failure.
int usb_diskio_register(USBStorageClient *client);
void usb_diskio_unregister(int drive);

}  // namespace esphome::usb_storage

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
