#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_helpers.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_timer.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "usb/usb_types_stack.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>

namespace esphome {
namespace usb_msc_host {

static constexpr char *const MNT_PATH = "/usb";
static constexpr uint8_t BUFFER_SIZE = 4096;
static constexpr uint8_t MAX_MSC_DEVICES = CONFIG_FATFS_VOLUME_COUNT;

/**
 * @brief MSC Device Entry
 *
 * This structure holds information about a connected MSC device,
 * including the USB address, MSC device handle, VFS handle, and assigned mount point.
 */
typedef struct {
  uint8_t usb_addr;                    /*!< USB device address */
  msc_host_device_handle_t msc_device; /*!< Handle of the MSC device */
  msc_host_vfs_handle_t vfs_handle;    /*!< VFS handle assigned to the MSC device */
} msc_dev_entry_t;

static constexpr char *const TAG = "usb_msc_host";
static constexpr uint8_t SCSI_COMMAND_SET = 0x06;
static constexpr uint8_t BULK_ONLY_TRANSFER = 0x50;

class USBMscHost : public Component {
  friend class USBHost;
  friend class USBMscDevice;

 public:
  void setup() override;

 protected:
  void free_all_msc_devices(void);
  void free_msc_device(int slot);
  int8_t find_free_slot(void);
  esp_err_t allocate_new_msc_device(uint8_t new_dev_address);
  int8_t find_msc_device_slot(usb_device_handle_t device_handle);
  msc_host_device_handle_t get_handle_by_address(usb_device_handle_t handle);

  msc_dev_entry_t *msc_devices_[MAX_MSC_DEVICES] = {NULL};
};

class USBMscDevice : public usb_host::USBClient, public Parented<USBMscHost> {
  friend class USBHost;
  friend class USBMscHost;

 public:
  USBMscDevice(uint16_t vid, uint16_t pid) : usb_host::USBClient(vid, pid) {}
  void setup() override;
  void dump_config() override;
  void list_files();
  void speed_test();
  void file_operations();
  void print_device_info();
  uint8_t find_usb_addr_by_handle(msc_host_device_handle_t handle);

 protected:
  void disconnect() override;
  void on_connected() override;
};

}  // namespace usb_msc_host
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
