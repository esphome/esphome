#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "tinyusb_component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "tinyusb_default_config.h"
#ifdef USE_TINYUSB_KEYBOARD
#include "../tinyusb_keyboard/keyboard.h"
#endif

namespace esphome::tinyusb {

static const char *const TAG = "tinyusb";

// Runs on the TinyUSB task: only wake the main loop, which reads the state and runs
// the automations.
static void tinyusb_event_cb(tinyusb_event_t *event, void *arg) {
  if (event->id == TINYUSB_EVENT_ATTACHED || event->id == TINYUSB_EVENT_DETACHED) {
    static_cast<TinyUSB *>(arg)->enable_loop_soon_any_context();
  }
}

void TinyUSB::setup() {
  // Use the device's MAC address as its serial number if no serial number is defined
  if (this->string_descriptor_[SERIAL_NUMBER] == nullptr) {
    static char mac_addr_buf[MAC_ADDRESS_BUFFER_SIZE];
    get_mac_address_into_buffer(mac_addr_buf);
    this->string_descriptor_[SERIAL_NUMBER] = mac_addr_buf;
  }

  // Start from esp_tinyusb defaults to keep required task settings valid across esp_tinyusb updates.
  this->tusb_cfg_ = TINYUSB_DEFAULT_CONFIG();
  this->tusb_cfg_.port = TINYUSB_PORT_FULL_SPEED_0;
  this->tusb_cfg_.phy.skip_setup = false;
  // Without VBUS monitoring the OTG core only sees a cable pull as the bus going idle
  // (a suspend), so TinyUSB never reports a detach and stays "mounted".
  if (this->vbus_monitor_pin_ >= 0) {
    this->tusb_cfg_.phy.self_powered = true;
    this->tusb_cfg_.phy.vbus_monitor_io = this->vbus_monitor_pin_;
  }
  this->tusb_cfg_.descriptor = {
      .device = &this->usb_descriptor_,
      .string = this->string_descriptor_,
      .string_count = SIZE,
  };

#ifdef USE_TINYUSB_KEYBOARD
  // esp_tinyusb requires a valid full-speed configuration descriptor when HID is enabled.
  // This is a minimal keyboard HID configuration descriptor, not much thought has been put into the contents.
  static const uint8_t fs_configuration_descriptor[] = {
      /* Configuration Descriptor (9) */
      0x09,       /* bLength */
      0x02,       /* bDescriptorType = Configuration */
      0x22, 0x00, /* wTotalLength = 34 (configuration + interface + HID + endpoint) */
      0x01,       /* bNumInterfaces */
      0x01,       /* bConfigurationValue */
      0x00,       /* iConfiguration */
      0x80,       /* bmAttributes (bus-powered) */
      0x32,       /* bMaxPower (100 mA) */

      /* Interface Descriptor (9) */
      0x09, /* bLength */
      0x04, /* bDescriptorType = Interface */
      0x00, /* bInterfaceNumber */
      0x00, /* bAlternateSetting */
      0x01, /* bNumEndpoints */
      0x03, /* bInterfaceClass = HID */
      0x01, /* bInterfaceSubClass = Boot */
      0x01, /* bInterfaceProtocol = Keyboard */
      0x00, /* iInterface (no string descriptor) */

      /* HID Descriptor (9) */
      0x09,       /* bLength */
      0x21,       /* bDescriptorType = HID */
      0x11, 0x01, /* bcdHID = 1.11 */
      0x00,       /* bCountryCode */
      0x01,       /* bNumDescriptors */
      0x22,       /* bDescriptorType (Report) */
      (uint8_t) (sizeof(esphome::tinyusb_keyboard::HID_REPORT_DESCRIPTOR) & 0xFF),
      (uint8_t) (sizeof(esphome::tinyusb_keyboard::HID_REPORT_DESCRIPTOR) >> 8), /* wDescriptorLength */

      /* Endpoint Descriptor (7) */
      0x07,       /* bLength */
      0x05,       /* bDescriptorType = Endpoint */
      0x81,       /* bEndpointAddress (IN endpoint 1) */
      0x03,       /* bmAttributes = Interrupt */
      0x08, 0x00, /* wMaxPacketSize = 8 */
      0x0A        /* bInterval = 10 ms */
  };

  this->tusb_cfg_.descriptor.full_speed_config = fs_configuration_descriptor;
#endif  // TINYUSB_KEYBOARD

  // Defense-in-depth: esp_tinyusb's tinyusb_descriptors_set() fails with
  // ESP_ERR_INVALID_ARG when no configuration descriptor is provided and
  // no class that has a built-in default (CDC/MSC/NCM) is compiled in. In
  // that case the internal task exits without notifying us, and
  // tinyusb_driver_install() blocks 5s on the notify-take -- long enough
  // to trip the task watchdog. Bail early so the rest of the device can
  // still boot.
#if !(CFG_TUD_CDC > 0 || CFG_TUD_MSC > 0 || CFG_TUD_NCM > 0)
  if (this->tusb_cfg_.descriptor.full_speed_config == nullptr) {
    ESP_LOGE(TAG, "No USB class configured");
    this->mark_failed();
    return;
  }
#endif

  this->tusb_cfg_.event_cb = tinyusb_event_cb;
  this->tusb_cfg_.event_arg = this;
  esp_err_t result = tinyusb_driver_install(&this->tusb_cfg_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(result));
    this->mark_failed();
    return;
  }
  // loop() only reports mount changes; the mount hooks wake it when one happens.
  this->disable_loop();
}

void TinyUSB::loop() {
  const bool mounted = tud_mounted();
  if (mounted != this->last_reported_mounted_) {
    this->last_reported_mounted_ = mounted;
    ESP_LOGD(TAG, "USB host %s", mounted ? LOG_STR_LITERAL("mounted") : LOG_STR_LITERAL("unmounted"));
    this->mount_state_callback_.call(mounted);
  }
  this->disable_loop();
}

void TinyUSB::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TinyUSB:\n"
                "  Product ID: 0x%04X\n"
                "  Vendor ID: 0x%04X\n"
                "  Manufacturer: '%s'\n"
                "  Product: '%s'\n"
                "  Serial: '%s'",
                this->usb_descriptor_.idProduct, this->usb_descriptor_.idVendor, this->string_descriptor_[MANUFACTURER],
                this->string_descriptor_[PRODUCT], this->string_descriptor_[SERIAL_NUMBER]);
  if (this->vbus_monitor_pin_ >= 0) {
    ESP_LOGCONFIG(TAG, "  VBUS Monitor Pin: GPIO%d", this->vbus_monitor_pin_);
  }
}

}  // namespace esphome::tinyusb
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
