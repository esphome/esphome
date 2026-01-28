#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tinyusb_keyboard {

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

template<typename... Ts> class PressAction : public Action<Ts...> {
 public:
  explicit PressAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, key)
  TEMPLATABLE_VALUE(uint32_t, key_code)
  TEMPLATABLE_VALUE(uint32_t, modifiers)

  void play(const Ts &...x) override {
    ESP_LOGD("tinyusb_keyboard", "PressAction fired");
    if (this->key_code_.has_value()) {
      uint32_t code = this->key_code_.value(x...);
      uint32_t mods = this->modifiers_.value_or(x..., 0);
      this->parent_->press_key((uint8_t) code, (uint8_t) mods);
      return;
    }
    if (this->key_.has_value()) {
      auto s = this->key_.value(x...);
      if (!s.empty())
        this->parent_->set_key(s);
    }
  }

 protected:
  TinyUSBKeyboard *parent_;
};

template<typename... Ts> class ReleaseAction : public Action<Ts...> {
 public:
  explicit ReleaseAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, key)
  TEMPLATABLE_VALUE(uint32_t, key_code)

  void play(const Ts &...x) override {
    ESP_LOGD("tinyusb_keyboard", "ReleaseAction fired");
    if (this->key_code_.has_value()) {
      uint32_t code = this->key_code_.value(x...);
      this->parent_->release_key((uint8_t) code);
      return;
    }
    if (this->key_.has_value()) {
      auto s = this->key_.value(x...);
      if (!s.empty())
        this->parent_->set_key(s);  // set_key does press+release for single char
    }
  }

 protected:
  TinyUSBKeyboard *parent_;
};

template<typename... Ts> class TypeAction : public Action<Ts...> {
 public:
  explicit TypeAction(TinyUSBKeyboard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, text)

  void play(const Ts &...x) override {
    ESP_LOGD("tinyusb_keyboard", "TypeAction fired");
    if (this->text_.has_value()) {
      auto s = this->text_.value(x...);
      this->parent_->set_text(s);
    }
  }

 protected:
  TinyUSBKeyboard *parent_;
};

#endif  // defined(USE_ESP32_VARIANT_...)

}  // namespace tinyusb_keyboard
}  // namespace esphome
