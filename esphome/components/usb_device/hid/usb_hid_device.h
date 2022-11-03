#pragma once
#include "esphome/core/defines.h"
#include "esphome/components/hid/hid_device.h"
#ifdef USE_ESP32_VARIANT_ESP32S2
#include "Adafruit_TinyUSB.h"
#include "esphome/core/component.h"
#ifdef USE_KEYBOARD
#include "usb_keyboard.h"
#endif

namespace esphome {
namespace usb_device {

class USBHIDDevice : public Component, public hid::HIDDevice {
 public:
  USBHIDDevice(Adafruit_USBD_HID *usb_hid) : usb_hid_(usb_hid) {}
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config();
#ifdef USE_KEYBOARD
  void set_report(KeyboardReport *keyboard_report) { keyboard_report_ = keyboard_report; }
  void set_report(MediaKeysReport *media_keys_report) { media_keys_report_ = media_keys_report; }
  KeyboardReport *keyboard_control() { return keyboard_report_; }
  MediaKeysReport *media_keys_control() { return media_keys_report_; }
#endif
 protected:
  Adafruit_USBD_HID *usb_hid_;
  static void hid_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
                                  uint16_t bufsize);
#ifdef USE_KEYBOARD
  KeyboardReport *keyboard_report_{nullptr};
  MediaKeysReport *media_keys_report_{nullptr};
#endif
};

}  // namespace usb_device
}  // namespace esphome

#endif
