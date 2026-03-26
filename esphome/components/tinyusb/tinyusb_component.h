#pragma once
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/component.h"

#include "tinyusb.h"
#include "tusb.h"

namespace esphome::tinyusb {

enum USBDStringDescriptor : uint8_t {
  LANGUAGE_ID = 0,
  MANUFACTURER = 1,
  PRODUCT = 2,
  SERIAL_NUMBER = 3,
  INTERFACE = 4,
  TERMINATOR = 5,
  SIZE = 6,
};

static const char *DEFAULT_USB_STR = "ESPHome";

class TinyUSB : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_usb_desc_product_id(uint16_t product_id) { this->usb_descriptor_.idProduct = product_id; }
  void set_usb_desc_vendor_id(uint16_t vendor_id) { this->usb_descriptor_.idVendor = vendor_id; }
  void set_usb_desc_lang_id(uint16_t lang_id) {
    this->usb_desc_lang_id_[0] = lang_id & 0xFF;
    this->usb_desc_lang_id_[1] = lang_id >> 8;
  }
  void set_usb_desc_manufacturer(const char *usb_desc_manufacturer) {
    this->string_descriptor_[MANUFACTURER] = usb_desc_manufacturer;
  }
  void set_usb_desc_product(const char *usb_desc_product) { this->string_descriptor_[PRODUCT] = usb_desc_product; }
  void set_usb_desc_serial(const char *usb_desc_serial) { this->string_descriptor_[SERIAL_NUMBER] = usb_desc_serial; }

 protected:
  char usb_desc_lang_id_[2] = {0x09, 0x04};  // defaults to english

  const char *string_descriptor_[SIZE] = {
      this->usb_desc_lang_id_,  // 0: supported language is English (0x0409)
      DEFAULT_USB_STR,          // 1: Manufacturer
      DEFAULT_USB_STR,          // 2: Product
      nullptr,                  // 3: Serial Number
      nullptr,                  // 4: Interface
      nullptr,                  // 5: Terminator
  };

  tinyusb_config_t tusb_cfg_{};
  tusb_desc_device_t usb_descriptor_{
      .bLength = sizeof(tusb_desc_device_t),
      .bDescriptorType = TUSB_DESC_DEVICE,
      .bcdUSB = 0x0200,
      .bDeviceClass = TUSB_CLASS_MISC,
      .bDeviceSubClass = MISC_SUBCLASS_COMMON,
      .bDeviceProtocol = MISC_PROTOCOL_IAD,
      .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
      .idVendor = 0x303A,
      .idProduct = 0x4001,
      .bcdDevice = CONFIG_TINYUSB_DESC_BCD_DEVICE,
      .iManufacturer = 1,
      .iProduct = 2,
      .iSerialNumber = 3,
      .bNumConfigurations = 1,
  };
};

}  // namespace esphome::tinyusb
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
