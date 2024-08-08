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

template<typename... Ts> class SetTextAction : public Action<Ts...> {
 public:
  explicit SetTextAction(MAX6921Component *max9621) : max9621_(max9621) {}

  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(uint8_t, text_position)
  TEMPLATABLE_VALUE(std::string, text_align)
  TEMPLATABLE_VALUE(std::string, text_effect)
  TEMPLATABLE_VALUE(uint8_t, text_effect_cycle_num)
  TEMPLATABLE_VALUE(uint32_t, text_effect_duration)
  TEMPLATABLE_VALUE(uint32_t, text_effect_update_interval)

  void play(Ts... x) override {
    auto pos = this->text_position_.value(x...);
    auto cycle_num = this->text_effect_cycle_num_.value(x...);
    auto duration = this->text_effect_duration_.value(x...);
    auto update_interval = this->text_effect_update_interval_.value(x...);
    this->max9621_->display_->set_text(this->text_.value(x...), pos, this->text_align_.value(x...), duration,
                                       this->text_effect_.value(x...), update_interval, cycle_num);
  }

 protected:
  MAX6921Component *max9621_;
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
  TEMPLATABLE_VALUE(uint32_t, demo_update_interval)
  TEMPLATABLE_VALUE(uint8_t, demo_cycle_num)

  // overlay to cover string inputs
  // void set_mode(const std::string mode);
  // void set_mode(const std::string mode) {
  //   if (str_equals_case_insensitive(mode, "off")) {
  //     this->set_mode(DEMO_MODE_OFF);
  //   } else if (str_equals_case_insensitive(mode, "scroll_font")) {
  //     this->set_mode(DEMO_MODE_SCROLL_FONT);
  //   } else {
  //     output log message (TAG, "Invalid demo mode %s", mode.c_str());
  //   }
  // }

  void play(Ts... x) override {
    auto update_interval = this->demo_update_interval_.value(x...);
    auto cycle_num = this->demo_cycle_num_.value(x...);
    this->max9621_->display_->set_demo_mode(this->mode_.value(x...), update_interval, cycle_num);
  }

 protected:
  // TemplatableValue<const std::string, Ts...> mode{};
  MAX6921Component *max9621_;
  // DemoMode mode_;
};

}  // namespace max6921
}  // namespace esphome
