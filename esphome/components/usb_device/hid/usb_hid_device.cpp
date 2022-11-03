#ifdef USE_ESP32_VARIANT_ESP32S2
#include "usb_hid_device.h"
#include "esphome/core/log.h"

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

USBHIDDevice *global_usb_hid_device;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void USBHIDDevice::setup() {
  global_usb_hid_device = this;
  usb_hid_->setReportCallback(NULL, hid_report_callback);
  usb_hid_->begin();
}

void USBHIDDevice::loop() {
#ifdef USE_KEYBOARD
  if (keyboard_report_) {
    keyboard_report_->loop();
  }
  if (media_keys_report_) {
    media_keys_report_->loop();
  }
#endif
}

float USBHIDDevice::get_setup_priority() const { return setup_priority::HARDWARE; }

void USBHIDDevice::dump_config() {
  ESP_LOGCONFIG(TAG, "HID is ready: %s", YESNO(usb_hid_->ready()));
  bool keyboard_exists = false;
  bool media_keys_exists = false;
#ifdef USE_KEYBOARD
  keyboard_exists = keyboard_report_ != nullptr;
  media_keys_exists = media_keys_report_ != nullptr;
#endif
  ESP_LOGCONFIG(TAG, "Keyboard exists: %s, media keys exists: %s", YESNO(keyboard_exists), YESNO(media_keys_exists));
}

void USBHIDDevice::hid_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
                                       uint16_t bufsize) {
  ESP_LOGV(TAG, "hid_report_callback - report_id %d, report_type %d, bufsize %d", report_id, report_type, bufsize);
  (void) report_id;
  (void) bufsize;
  // LED indicator is output report with only 1 byte length
  if (report_type != HID_REPORT_TYPE_OUTPUT)
    return;
  // The LED bit map is as follows: (also defined by KEYBOARD_LED_* )
  // Kana (4) | Compose (3) | ScrollLock (2) | CapsLock (1) | Numlock (0)
  uint8_t led_indicator = buffer[0];
#ifdef USE_KEYBOARD
  if (global_usb_hid_device && global_usb_hid_device->keyboard_report_) {
    global_usb_hid_device->keyboard_report_->set_led_indicator(led_indicator);
  }
#endif
}

}  // namespace usb_device
}  // namespace esphome

#endif
