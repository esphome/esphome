#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP32_VARIANT_ESP32S2
#ifdef USE_KEYBOARD

#include <functional>
#include "Adafruit_TinyUSB.h"
#include "esphome/components/hid/keyboard/hid_keyboard.h"
#include "esphome/components/hid/keyboard/hid_media_keys.h"

namespace esphome {
namespace usb_device {

template<class T> class Report : public T {
 public:
  explicit Report(Adafruit_USBD_HID *usb_hid, const std::function<bool()> &report)
      : usb_hid_(usb_hid), report_(report) {}
  virtual void loop();
  void report() override;

 protected:
  bool pending_;
  std::function<bool()> report_;
  Adafruit_USBD_HID *usb_hid_;
};

class KeyboardReport : public Report<hid::Keyboard> {
 public:
  KeyboardReport(Adafruit_USBD_HID *usb_hid, uint8_t report_id);
  void loop() override;
  void set_led_indicator(uint8_t led_indicator) {
    led_indicator_ = led_indicator;
    update_led_ = true;
  }

 protected:
  uint8_t led_indicator_{0};
  bool update_led_{false};
};

class MediaKeysReport : public Report<hid::MediaKeys> {
 public:
  MediaKeysReport(Adafruit_USBD_HID *usb_hid, uint8_t report_id);
};

}  // namespace usb_device
}  // namespace esphome

#endif
#endif
