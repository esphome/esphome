#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "keyboard.h"
#include "esphome/core/log.h"

namespace esphome::tinyusb_keyboard {

template<typename... Ts> class PressAction : public Action<Ts...> {
 public:
  explicit PressAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, key)
  TEMPLATABLE_VALUE(uint8_t, key_code)
  TEMPLATABLE_VALUE(uint8_t, modifiers)

  void play(const Ts &...x) override {
    if (this->key_code_.has_value()) {
      uint8_t code = this->key_code_.value(x...);
      uint8_t mods = this->modifiers_.value_or(x..., 0);
      this->parent_->press_key(code, mods);
      return;
    }
    if (this->key_.has_value()) {
      auto s = this->key_.value(x...);
      uint8_t mods = this->modifiers_.value_or(x..., 0);
      if (!s.empty()) {
        this->parent_->press_key(0x04 + (s[0] - 'a'), mods);
      }
    }
  }

 protected:
  TinyUSBKeyboard *parent_;
};

template<typename... Ts> class ReleaseAction : public Action<Ts...> {
 public:
  explicit ReleaseAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  void play(const Ts &...x) override { this->parent_->release_keys(); }

 protected:
  TinyUSBKeyboard *parent_;
};

template<typename... Ts> class MediaPressAction : public Action<Ts...> {
 public:
  explicit MediaPressAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint16_t, usage)

  void play(const Ts &...x) override {
    if (this->usage_.has_value()) {
      uint16_t usage = this->usage_.value(x...);
      this->parent_->press_media(usage);
    }
  }

 protected:
  TinyUSBKeyboard *parent_;
};

template<typename... Ts> class MediaReleaseAction : public Action<Ts...> {
 public:
  explicit MediaReleaseAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  void play(const Ts &...x) override { this->parent_->release_media(); }

 protected:
  TinyUSBKeyboard *parent_;
};

}  // namespace esphome::tinyusb_keyboard

#endif  // defined(USE_ESP32_VARIANT_...)
