#pragma once
#ifdef USE_ZEPHYR

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <atomic>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>

namespace esphome::usb_hid_keyboard {

// Standard 8-byte boot-keyboard report layout:
//   byte 0: modifier bitmask  (Left/Right Ctrl/Shift/Alt/GUI)
//   byte 1: reserved (always 0)
//   bytes 2-7: up to six simultaneous key codes
struct KeyboardReport {
  uint8_t modifier{0};
  uint8_t reserved{0};
  uint8_t keys[6]{};
};

class USBHIDKeyboard : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void press_key(uint8_t modifier, uint8_t keycode);
  void release_all();
  void set_ep_ready(bool ready) { this->ep_ready_.store(ready, std::memory_order_release); }

  static void int_in_ready_cb(const struct device *dev);

 protected:
  void send_report_(const KeyboardReport &report);

  const struct device *hid_dev_{nullptr};
  std::atomic<bool> ep_ready_{false};
};

template<typename... Ts> class KeyPressAction : public Action<Ts...> {
 public:
  explicit KeyPressAction(USBHIDKeyboard *parent) : parent_(parent) {}

  void set_keycode(uint8_t keycode) { this->keycode_ = keycode; }
  void set_modifier(uint8_t modifier) { this->modifier_ = modifier; }

  void play(const Ts &...x) override { this->parent_->press_key(this->modifier_, this->keycode_); }

 protected:
  USBHIDKeyboard *parent_;
  uint8_t keycode_{0};
  uint8_t modifier_{0};
};

template<typename... Ts> class KeyReleaseAction : public Action<Ts...> {
 public:
  explicit KeyReleaseAction(USBHIDKeyboard *parent) : parent_(parent) {}

  void play(const Ts &...x) override { this->parent_->release_all(); }

 protected:
  USBHIDKeyboard *parent_;
};

extern USBHIDKeyboard *global_usb_hid_keyboard;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::usb_hid_keyboard
#endif  // USE_ZEPHYR
