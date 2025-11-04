#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
//#include "esphome/components/usb_host/usb_host.h"
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

static const char *const TAG = "usb_msc_host";

class USBMscHost : public Component {
  friend class USBHost;

 public:
  void setup() override;
  void dump_config() override;
};

class USBMscDevice : public Component {
  friend class USBHost;

 public:
  USBMscDevice() : Component() {}
  void setup() override;
  void dump_config() override;

 protected:
  void disconnect();
  void on_connected();
};
/*
class USBMscDevice : public usb_host::USBClient {
  friend class USBHost;

 public:
  USBMscDevice(uint16_t vid, uint16_t pid) : usb_host::USBClient(vid, pid) {}
  void setup() override;
  void dump_config() override;

 protected:
  void disconnect() override;
  void on_connected() override;
};
*/

}  // namespace usb_msc_host
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
