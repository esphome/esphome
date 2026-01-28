#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

extern "C" {
// TinyUSB HID API
#include "tusb.h"
#include "class/hid/hid_device.h"
// FreeRTOS for vTaskDelay
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

namespace esphome {
namespace tinyusb_keyboard {

static const char *TAG = "tinyusb_keyboard";

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
static const uint8_t HID_REPORT_DESCRIPTOR[] = {
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

void TinyUSBKeyboard::setup() {
  // If TinyUSB is not initialized, we can't operate. We rely on tinyusb component
  // to install the driver. Check tud_ready().
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready at setup(); keyboard will wait until ready.");
    this->ready_ = false;
  } else {
    this->ready_ = true;
    ESP_LOGI(TAG, "TinyUSB keyboard ready");
  }
}

void TinyUSBKeyboard::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TinyUSB Keyboard component:\n"
                "  Ready at setup(): %s\n"
                "  tud_ready(): %s\n",
                this->ready_ ? "YES" : "NO", tud_ready() ? "YES" : "NO");
}

void TinyUSBKeyboard::press_key(uint8_t keycode, uint8_t modifiers) {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping press_key");
    return;
  }

  ESP_LOGD(TAG, "press_key keycode=0x%02X modifiers=0x%02X", keycode, modifiers);

  // Prepare keyboard report: [modifier][reserved][k1..k6]
  uint8_t report[8] = {0};
  report[0] = modifiers;
  report[1] = 0x00;  // reserved
  report[2] = keycode;
  // Send using Report ID 1 (keyboard)
  tud_hid_report(1, report, sizeof(report));
}

void TinyUSBKeyboard::press_media(uint16_t usage) {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping press_media");
    return;
  }
  ESP_LOGI(TAG, "press_media usage=0x%04X tud_ready=%d", usage, tud_ready());
  // Consumer reports are 2 bytes (usage code); send with Report ID 2
  uint8_t report[2] = {(uint8_t) (usage & 0xFF), (uint8_t) ((usage >> 8) & 0xFF)};
  tud_hid_report(2, report, sizeof(report));
}

void TinyUSBKeyboard::release_media() {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping release_media");
    return;
  }
  ESP_LOGI(TAG, "release_media tud_ready=%d", tud_ready());
  uint8_t report[2] = {0, 0};
  tud_hid_report(2, report, sizeof(report));
}

void TinyUSBKeyboard::release_key(uint8_t keycode) {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping release_key");
    return;
  }
  // Release all keys by sending empty keyboard report (Report ID 1)
  ESP_LOGD(TAG, "release_key keycode=0x%02X", keycode);
  uint8_t report[8] = {0};
  tud_hid_report(1, report, sizeof(report));
}

void TinyUSBKeyboard::type_string(const char *text) {
  if (text == nullptr)
    return;
  // Simple blocking helper: send each ASCII character as a basic HID key (US layout)
  for (const char *p = text; *p != '\0'; ++p) {
    char c = *p;
    uint8_t modifiers = 0;
    uint8_t keycode = 0;
    // Very small ASCII -> HID mapping for letters and space
    if (c >= 'a' && c <= 'z') {
      keycode = 0x04 + (c - 'a');
    } else if (c >= 'A' && c <= 'Z') {
      keycode = 0x04 + (c - 'A');
      modifiers = 0x02;  // Left Shift
    } else if (c == ' ') {
      keycode = 0x2C;  // space
    } else if (c >= '1' && c <= '9') {
      keycode = 0x1E + (c - '1');
    } else if (c == '0') {
      keycode = 0x27;
    } else {
      // Unsupported character: skip
      continue;
    }

    this->press_key(keycode, modifiers);
    vTaskDelay(pdMS_TO_TICKS(10));
    this->release_key(keycode);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void TinyUSBKeyboard::set_key_code(uint32_t code) { this->press_key((uint8_t) code); }

void TinyUSBKeyboard::set_modifiers(uint32_t mods) { (void) mods; }

void TinyUSBKeyboard::set_text(const std::string &text) { this->type_string(text.c_str()); }

}  // namespace tinyusb_keyboard
}  // namespace esphome

extern "C" {
// TinyUSB expects the application to provide these callbacks when HID is enabled.
// Provide minimal stubs so linking succeeds. Implementations can be expanded later.
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void) instance;
  return ::esphome::tinyusb_keyboard::HID_REPORT_DESCRIPTOR;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}
}  // extern "C"

#endif  // defined(USE_ESP32_VARIANT...)
