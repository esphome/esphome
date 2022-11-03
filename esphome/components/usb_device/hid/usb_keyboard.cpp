#include "usb_keyboard.h"
#ifdef USE_ESP32_VARIANT_ESP32S2
#ifdef USE_KEYBOARD
#include "esphome/core/log.h"

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

template<class T> void Report<T>::loop() {
  if (pending_) {
    report();
  }
}

template<class T> void Report<T>::report() {
  if (usb_hid_->ready() == false) {
    if (!pending_) {
      ESP_LOGD(TAG, "HID device is not ready");
    }
    pending_ = true;
    return;
  }
  if (report_()) {
    pending_ = false;
  } else {
    pending_ = true;
    ESP_LOGW(TAG, "HID device report was not sent");
  }
}

KeyboardReport::KeyboardReport(Adafruit_USBD_HID *usb_hid, uint8_t report_id)
    : Report(usb_hid, [this, report_id] {
        ESP_LOGD(TAG, "keyboard report id: %d - modifier: %d, code %d, %d, %d, %d, %d, %d", report_id, modifier_,
                 hidcode_[0], hidcode_[1], hidcode_[2], hidcode_[3], hidcode_[4], hidcode_[5]);
        return usb_hid_->keyboardReport(report_id, modifier_, hidcode_);
      }) {}

void KeyboardReport::loop() {
  Report::loop();
#ifdef USE_BINARY_SENSOR
  // those are updated from 2 different threads but it does not matter much
  if (led_control_ && update_led_) {
    update_led_ = false;
    led_control_->publish_capslock(led_indicator_ & KEYBOARD_LED_CAPSLOCK);
    led_control_->publish_numlock(led_indicator_ & KEYBOARD_LED_NUMLOCK);
    led_control_->publish_scrollock(led_indicator_ & KEYBOARD_LED_SCROLLLOCK);
  }
#endif
}

MediaKeysReport::MediaKeysReport(Adafruit_USBD_HID *usb_hid, uint8_t report_id)
    : Report(usb_hid, [this, report_id] {
        ESP_LOGD(TAG, "media keys report id: %d - %d", report_id, media_keys_);
        return usb_hid_->sendReport16(report_id, media_keys_);
      }) {}

template class Report<hid::Keyboard>;
template class Report<hid::MediaKeys>;

}  // namespace usb_device
}  // namespace esphome
#endif
#endif
