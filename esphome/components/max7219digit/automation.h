#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "max7219digit.h"

namespace esphome::max7219digit {

template<typename... Ts> class DisplayInvertAction final : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(const Ts &...x) override {
    bool state = this->state_.value(x...);
    this->parent_->invert_on_off(state);
  }
};

template<typename... Ts> class DisplayVisibilityAction final : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(const Ts &...x) override {
    bool state = this->state_.value(x...);
    this->parent_->turn_on_off(state);
  }
};

template<typename... Ts> class DisplayReverseAction final : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(const Ts &...x) override {
    bool state = this->state_.value(x...);
    this->parent_->set_reverse(state);
  }
};

template<typename... Ts> class DisplayIntensityAction final : public Action<Ts...>, public Parented<MAX7219Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, state)

  void play(const Ts &...x) override {
    uint8_t state = this->state_.value(x...);
    this->parent_->set_intensity(state);
  }
};

}  // namespace esphome::max7219digit
