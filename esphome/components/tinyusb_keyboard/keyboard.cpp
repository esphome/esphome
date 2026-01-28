#include "keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

// Guard the entire implementation for supported ESP32 variants in one place
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

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

// Standard HID report descriptor for a boot keyboard (modifier + reserved + 6 keycodes + LED output)
static const uint8_t HID_REPORT_DESCRIPTOR[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
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
    0xC0        /* End Collection */
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

  // Prepare 6-key rollover array
  uint8_t keys[6] = {0};
  keys[0] = keycode;
  // Use report id 0 and pass key array pointer. Managed TinyUSB headers expect (report_id, modifier, keycode[]).
  tud_hid_keyboard_report(0, modifiers, keys);
}

void TinyUSBKeyboard::release_key(uint8_t keycode) {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping release_key");
    return;
  }
  // Release all keys by sending empty report
  ESP_LOGD(TAG, "release_key keycode=0x%02X", keycode);
  uint8_t empty[6] = {0};
  tud_hid_keyboard_report(0, 0, empty);
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

// Implementation of setters used by codegen action objects
void TinyUSBKeyboard::set_key(const std::string &key) {
  if (key.length() == 1) {
    char c = key[0];
    if (c >= 'a' && c <= 'z') {
      this->press_key(0x04 + (c - 'a'));
      this->release_key(0x04 + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
      this->press_key(0x04 + (c - 'A'), 0x02);
      this->release_key(0x04 + (c - 'A'));
    }
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
  // Return pointer to our static HID report descriptor so the host can enumerate a
  // standard keyboard. The managed tinyusb component provides the configuration
  // descriptor; this callback supplies the HID report descriptor itself.
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
