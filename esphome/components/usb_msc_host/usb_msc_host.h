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

static constexpr const char *MNT_PATH = "/usb";
static constexpr uint16_t BUFFER_SIZE = 4096;
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

static constexpr const char *TAG = "usb_msc_host";
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
  esp_err_t allocate_new_msc_device(uint8_t new_dev_address, const std::string &mount_path);
  int8_t find_msc_device_slot(uint8_t usb_addr);
  msc_host_device_handle_t get_handle_by_address(uint8_t usb_addr);

  msc_dev_entry_t *msc_devices_[MAX_MSC_DEVICES] = {NULL};
};

class USBMscDevice : public Component, public usb_host::USBDeviceHandler, public Parented<USBMscHost> {
  friend class USBHost;
  friend class USBMscHost;

 public:
  USBMscDevice() = default;
  void setup() override;
  void dump_config() override;

  void set_usb_host(usb_host::USBHost *usb_host) { this->usb_host_ = usb_host; }
  void set_mount_path(const std::string &mount_path) { this->mount_path_ = mount_path; }
  void set_vid(uint16_t vid) { this->vid_ = vid; }
  void set_pid(uint16_t pid) { this->pid_ = pid; }

  // USBDeviceHandler Interface implementation
  bool matches_device(const usb_config_desc_t *config_desc) override;
  void on_device_connected(usb_device_handle_t device_handle, uint8_t addr) override;
  void on_device_disconnected(usb_device_handle_t device_handle) override;

  // MSC-specific operations
  void list_files();
  void speed_test();
  void file_operations();
  void print_device_info();
  uint8_t find_usb_addr_by_handle(msc_host_device_handle_t handle);

 protected:
  usb_device_handle_t device_handle_{nullptr};
  uint8_t device_addr_{255};
  usb_host::USBHost *usb_host_{nullptr};
  std::string mount_path_;
  uint16_t vid_{0x0000};  // 0x0000 = wildcard, match any VID
  uint16_t pid_{0x0000};  // 0x0000 = wildcard, match any PID
  int8_t slot_{-1};       // Track which slot this device is using
};

}  // namespace usb_msc_host
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
