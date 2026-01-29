// Minimal TinyUSB keyboard C++ interface (gated for supported ESP32 variants)
#pragma once

#include "esphome/core/component.h"
#include <cstdint>

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

namespace esphome {
namespace tinyusb_keyboard {

class TinyUSBKeyboard : public Component {
 public:
  void setup() override;
  void dump_config() override;

  // keycode is HID usage ID (USB HID), modifiers is bitmask (e.g., 0x02 for Shift)
  void press_key(uint8_t keycode, uint8_t modifiers = 0);
  void release_key(uint8_t keycode);
  // Consumer (media) key support (HID Consumer usages, e.g., volume up/down)
  void press_media(uint16_t usage);
  void release_media();

 protected:
  bool ready_{false};
};

}  // namespace tinyusb_keyboard
}  // namespace esphome

#endif  // defined(USE_ESP32_VARIANT_...)
