#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "display.h"
#include "esphome/core/automation.h"
#include "max6921.h"

namespace esphome {
namespace max6921 {

class Max6921Display;

template<typename... Ts> class SetTextAction : public Action<Ts...> {
 public:
  explicit SetTextAction(MAX6921Component *max9621) : max9621_(max9621) {}

  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(uint8_t, text_position)
  TEMPLATABLE_VALUE(std::string, text_align)
  TEMPLATABLE_VALUE(std::string, text_effect)
  TEMPLATABLE_VALUE(uint8_t, text_effect_cycle_num)
  TEMPLATABLE_VALUE(uint32_t, text_effect_duration)
  TEMPLATABLE_VALUE(uint8_t, text_repeat_num)
  TEMPLATABLE_VALUE(uint32_t, text_repeat_interval)
  TEMPLATABLE_VALUE(uint32_t, text_effect_update_interval)

  void play(Ts... x) override {
    auto pos = this->text_position_.value(x...);
    auto effect_cycle_num = this->text_effect_cycle_num_.value(x...);
    auto duration = this->text_effect_duration_.value(x...);
    auto repeat_num = this->text_repeat_num_.value(x...);
    auto repeat_interval = this->text_repeat_interval_.value(x...);
    auto update_interval = this->text_effect_update_interval_.value(x...);
    this->max9621_->display_->set_text(this->text_.value(x...), pos, this->text_align_.value(x...), duration,
                                       this->text_effect_.value(x...), effect_cycle_num, repeat_num, repeat_interval,
                                       update_interval);
  }

 protected:
  MAX6921Component *max9621_;
};

template<typename... Ts> class SetDemoModeAction : public Action<Ts...> {
 public:
  explicit SetDemoModeAction(MAX6921Component *max9621) : max9621_(max9621) {}

  TEMPLATABLE_VALUE(std::string, mode)
  TEMPLATABLE_VALUE(uint32_t, demo_update_interval)
  TEMPLATABLE_VALUE(uint8_t, demo_cycle_num)

  void play(Ts... x) override {
    auto update_interval = this->demo_update_interval_.value(x...);
    auto cycle_num = this->demo_cycle_num_.value(x...);
    this->max9621_->display_->set_demo_mode(this->mode_.value(x...), update_interval, cycle_num);
  }

 protected:
  MAX6921Component *max9621_;
};

}  // namespace max6921
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
