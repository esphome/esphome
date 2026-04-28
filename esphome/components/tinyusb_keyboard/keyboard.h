#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esphome/core/component.h"
#include <cstdint>

namespace esphome::tinyusb_keyboard {

class TinyUSBKeyboard : public Component {
 public:
  void setup() override;
  void dump_config() override;

  void press_key(uint8_t keycode, uint8_t modifiers = 0);
  void release_keys();
  void press_media(uint16_t usage);
  void release_media();

 protected:
  bool ready_{false};
};

}  // namespace esphome::tinyusb_keyboard

#endif  // defined(USE_ESP32_VARIANT_...)
