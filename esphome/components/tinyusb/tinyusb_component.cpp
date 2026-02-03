#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "tinyusb_component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tinyusb {

static const char *TAG = "tinyusb";

void TinyUSB::setup() {
  // Use the device's MAC address as its serial number if no serial number is defined
  if (this->string_descriptor_[SERIAL_NUMBER] == nullptr) {
    static char mac_addr_buf[13];
    get_mac_address_into_buffer(mac_addr_buf);
    this->string_descriptor_[SERIAL_NUMBER] = mac_addr_buf;
  }

  this->tusb_cfg_ = {
      .descriptor = &this->usb_descriptor_,
      .string_descriptor = this->string_descriptor_,
      .string_descriptor_count = SIZE,
      .external_phy = false,
  };

  esp_err_t result = tinyusb_driver_install(&this->tusb_cfg_);
  if (result != ESP_OK) {
    this->mark_failed();
  }
}

void TinyUSB::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TinyUSB:\n"
                "  Product ID: 0x%04X\n"
                "  Vendor ID: 0x%04X\n"
                "  Manufacturer: '%s'\n"
                "  Product: '%s'\n"
                "  Serial: '%s'\n",
                this->usb_descriptor_.idProduct, this->usb_descriptor_.idVendor, this->string_descriptor_[MANUFACTURER],
                this->string_descriptor_[PRODUCT], this->string_descriptor_[SERIAL_NUMBER]);
}

}  // namespace esphome::tinyusb
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
