#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "keyboard.h"
#include "esphome/core/log.h"

namespace esphome::tinyusb_keyboard {

ESPHOME_LOG_TAG(TAG, "tinyusb_keyboard");

void TinyUSBKeyboard::dump_config() { ESP_LOGCONFIG(TAG, "TinyUSB Keyboard"); }

void TinyUSBKeyboard::set_keyboard_(uint8_t keycode, uint8_t modifiers) {
  this->keycode_ = keycode;
  this->modifiers_ = modifiers;
  this->keyboard_pending_ = true;
  this->flush_();
}

void TinyUSBKeyboard::set_consumer_(uint16_t usage) {
  this->usage_ = usage;
  this->consumer_pending_ = true;
  this->flush_();
}

void TinyUSBKeyboard::flush_() {
  if (!tud_ready()) {
    // Nothing is listening; the state is sent once a host configures the device
    this->enable_loop();
    return;
  }
  // tud_hid_*_report() returns false while the endpoint still holds the previous report, which is
  // what happens for a press and a release in one action list. Keep retrying so the release gets out.
  if (this->keyboard_pending_) {
    uint8_t keycodes[6] = {this->keycode_};
    if (tud_hid_keyboard_report(REPORT_ID_KEYBOARD, this->modifiers_, keycodes)) {
      this->keyboard_pending_ = false;
    }
  }
  if (this->consumer_pending_ && !this->keyboard_pending_ &&
      tud_hid_report(REPORT_ID_CONSUMER, &this->usage_, sizeof(this->usage_))) {
    this->consumer_pending_ = false;
  }
  if (this->keyboard_pending_ || this->consumer_pending_) {
    this->enable_loop();
  } else {
    this->disable_loop();
  }
}

void TinyUSBKeyboard::loop() { this->flush_(); }

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
