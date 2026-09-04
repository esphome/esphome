#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esphome/core/component.h"
#include <cstdint>

namespace esphome::tinyusb_keyboard {

// Combined HID report descriptor for a composite device exposing both a
// Boot Keyboard (Report ID = 1) and a Consumer Control (media) interface
// (Report ID = 2). We present both top-level collections in a single
// report descriptor so the host enumerates one USB device with two reports
// identified by explicit Report IDs. The keyboard uses the standard Boot
// Keyboard layout (modifier byte, reserved, 6 keycode bytes, plus LED output
// report). The consumer control report is a single 16-bit usage value used
// for common media controls (for example: 0xE9 = Volume Up, 0xEA = Volume
// Down, 0xCD = Play/Pause).
//
// Report ID mapping:
//   Report ID 1 - Boot Keyboard (8 bytes: modifier, reserved, 6 keycodes)
//   Report ID 2 - Consumer Control (2 bytes: 16-bit usage code)
//
// Note: using explicit Report IDs avoids clashes between the two collections
// and makes it straightforward to send either report with tud_hid_report(report_id,...).
inline constexpr uint8_t HID_REPORT_DESCRIPTOR[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, 0x01, /*   Report ID (1) */
    0x05, 0x07, /*   Usage Page (Key Codes) */
    0x19, 0xE0, /*   Usage Minimum (224) */
    0x29, 0xE7, /*   Usage Maximum (231) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x01, /*   Logical Maximum (1) */
    0x75, 0x01, /*   Report Size (1) */
    0x95, 0x08, /*   Report Count (8) */
    0x81, 0x02, /*   Input (Data, Variable, Absolute) ; Modifier byte */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x08, /*   Report Size (8) */
    0x81, 0x01, /*   Input (Constant) ; Reserved byte */
    0x95, 0x05, /*   Report Count (5) */
    0x75, 0x01, /*   Report Size (1) */
    0x05, 0x08, /*   Usage Page (LEDs) */
    0x19, 0x01, /*   Usage Minimum (1) */
    0x29, 0x05, /*   Usage Maximum (5) */
    0x91, 0x02, /*   Output (Data, Variable, Absolute) ; LED report */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x03, /*   Report Size (3) */
    0x91, 0x01, /*   Output (Constant) ; Padding */
    0x95, 0x06, /*   Report Count (6) */
    0x75, 0x08, /*   Report Size (8) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x65, /*   Logical Maximum (101) */
    0x05, 0x07, /*   Usage Page (Key codes) */
    0x19, 0x00, /*   Usage Minimum (0) */
    0x29, 0x65, /*   Usage Maximum (101) */
    0x81, 0x00, /*   Input (Data, Array) ; Key arrays (6 bytes) */
    0xC0,       /* End Collection (Keyboard) */

    /* Consumer Control (media) top-level collection, Report ID 2 */
    0x05, 0x0C,       /* Usage Page (Consumer) */
    0x09, 0x01,       /* Usage (Consumer Control) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x02,       /*   Report ID (2) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x26, 0xFF, 0x03, /* Logical Maximum (0x03FF) */
    0x19, 0x00,       /* Usage Minimum (0) */
    0x2A, 0xFF, 0x03, /* Usage Maximum 0x03FF */
    0x75, 0x10,       /*   Report Size (16) */
    0x95, 0x01,       /*   Report Count (1) */
    0x81, 0x00,       /*   Input (Data, Variable, Absolute) */
    0xC0              /* End Collection (Consumer) */
};

class TinyUSBKeyboard : public Component {
 public:
  void dump_config() override;

  void press_key(uint8_t keycode, uint8_t modifiers = 0);
  void release_keys();
  void press_media(uint16_t usage);
  void release_media();
};

}  // namespace esphome::tinyusb_keyboard

#endif  // defined(USE_ESP32_VARIANT_...)
