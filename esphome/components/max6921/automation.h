#pragma once

#include "display.h"
#include "esphome/core/automation.h"
#include "max6921.h"

namespace esphome {
namespace max6921 {

class Display;

template<typename... Ts> class SetBrightnessAction : public Action<Ts...>, public Parented<MAX6921Component> {
 public:
  TEMPLATABLE_VALUE(float, brightness)

  void play(Ts... x) override { this->parent_->set_brightness(this->brightness_.value(x...)); }
};

#if 0
template<typename... Ts> class SetDemoModeAction : public Action<Ts...>, public Parented<MAX6921Component> {
 public:
  TEMPLATABLE_VALUE(DemoMode, mode)
  TEMPLATABLE_VALUE(uint8_t, cycle_num)

  void play(Ts... x) override {
    this->parent_->set_demo_mode(this->mode_.value(x...), this->cycle_num_.optional_value(x...));
  }
};
#endif


template<typename... Ts> class SetDemoModeAction : public Action<Ts...> {
 public:
  explicit SetDemoModeAction(MAX6921Component *max9621) : max9621_(max9621) {}

  TEMPLATABLE_VALUE(std::string, mode)
  TEMPLATABLE_VALUE(uint8_t, cycle_num)

  // overlay to cover string inputs
  // void set_mode(const std::string mode);
  // void set_mode(const std::string mode) {
  //   if (str_equals_case_insensitive(mode, "off")) {
  //     this->set_mode(DEMO_MODE_OFF);
  //   } else if (str_equals_case_insensitive(mode, "scroll_font")) {
  //     this->set_mode(DEMO_MODE_SCROLL_FONT);
    // } else {
    //   ESP_LOGW(TAG, "Invalid demo mode %s", mode.c_str());
    // }
  // }

  void play(Ts... x) override {
    auto cycle_num = this->cycle_num_.value(x...);
    this->max9621_->display_->set_demo_mode(this->mode_.value(x...), cycle_num);
  }

 protected:
  // TemplatableValue<const std::string, Ts...> mode{};
  MAX6921Component *max9621_;
  // DemoMode mode_;
};


template<typename... Ts> class SetTextAction : public Action<Ts...>, public Parented<MAX6921Component> {
 public:
  TEMPLATABLE_VALUE(std::string, text)

  void play(Ts... x) override { this->parent_->set_text(this->text_.value(x...)); }
};


}  // namespace max9621
}  // namespace esphome
