#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "tusb.h"

namespace esphome::tinyusb_keyboard {

static const char *TAG = "tinyusb_keyboard";

void TinyUSBKeyboard::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TinyUSB Keyboard component:\n"
                "  tud_ready(): %s\n",
                tud_ready() ? "YES" : "NO");
}

void TinyUSBKeyboard::press_key(uint8_t keycode, uint8_t modifiers) {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping press_key");
    return;
  }

  uint8_t report[8] = {0};
  report[0] = modifiers;
  report[1] = 0x00;  // reserved
  report[2] = keycode;
  // Send using Report ID 1 (keyboard)
  tud_hid_report(1, report, sizeof(report));
}

void TinyUSBKeyboard::release_keys() {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping release_keys");
    return;
  }
  // Release all keys by sending empty keyboard report (Report ID 1)
  uint8_t report[8] = {0};
  tud_hid_report(1, report, sizeof(report));
}

void TinyUSBKeyboard::press_media(uint16_t usage) {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping press_media");
    return;
  }
  // Consumer reports are 2 bytes (usage code); send with Report ID 2
  uint8_t report[2] = {(uint8_t) (usage & 0xFF), (uint8_t) ((usage >> 8) & 0xFF)};
  tud_hid_report(2, report, sizeof(report));
}

void TinyUSBKeyboard::release_media() {
  if (!tud_ready()) {
    ESP_LOGW(TAG, "TinyUSB not ready; dropping release_media");
    return;
  }
  uint8_t report[2] = {0, 0};
  tud_hid_report(2, report, sizeof(report));
}

}  // namespace esphome::tinyusb_keyboard

extern "C" {
// TinyUSB expects the application to provide these callbacks when HID is enabled.
// We don't actually use these yet.
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
