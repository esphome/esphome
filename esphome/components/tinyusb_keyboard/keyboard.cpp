#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "keyboard.h"
#include "esphome/core/log.h"

namespace esphome::tinyusb_keyboard {

ESPHOME_LOG_TAG(TAG, "tinyusb_keyboard");

void TinyUSBKeyboard::dump_config() { ESP_LOGCONFIG(TAG, "TinyUSB Keyboard"); }

bool TinyUSBKeyboard::ready_() {
  if (tud_ready()) {
    return true;
  }
  ESP_LOGW(TAG, "USB host not ready, dropping report");
  return false;
}

void TinyUSBKeyboard::press_key(uint8_t keycode, uint8_t modifiers) {
  uint8_t keycodes[6] = {keycode};
  if (this->ready_()) {
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifiers, keycodes);
  }
}

void TinyUSBKeyboard::release_keys() {
  if (this->ready_()) {
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, nullptr);
  }
}

void TinyUSBKeyboard::press_media(uint16_t usage) {
  if (this->ready_()) {
    tud_hid_report(REPORT_ID_CONSUMER, &usage, sizeof(usage));
  }
}

}  // namespace esphome::tinyusb_keyboard

// TinyUSB resolves these at link time when HID is enabled
extern "C" {
const uint8_t *tud_hid_descriptor_report_cb(uint8_t /*instance*/) {
  return esphome::tinyusb_keyboard::HID_REPORT_DESCRIPTOR;
}

uint16_t tud_hid_get_report_cb(uint8_t /*instance*/, uint8_t /*report_id*/, hid_report_type_t /*report_type*/,
                               uint8_t * /*buffer*/, uint16_t /*reqlen*/) {
  return 0;
}

// Keyboard LED state from the host; nothing consumes it yet
void tud_hid_set_report_cb(uint8_t /*instance*/, uint8_t /*report_id*/, hid_report_type_t /*report_type*/,
                           const uint8_t * /*buffer*/, uint16_t /*bufsize*/) {}
}

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
