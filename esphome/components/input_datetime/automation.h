#pragma once

#include "esphome/components/input_datetime/input_datetime.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace input_datetime {

static const char *const TAG = "input_datetime.automation";

class InputDatetimeOnTimeTrigger : public Trigger<>, public Component {
 public:
  explicit InputDatetimeOnTimeTrigger(InputDatetime *inputDatetime, time::RealTimeClock *rtc)
      : inputDatetime_(inputDatetime), rtc_(rtc) {}

  bool matches(const ESPTime &time);

  void loop() override;
  float get_setup_priority() const override;

 protected:
  time::RealTimeClock *rtc_;
  optional<ESPTime> last_check_;
  InputDatetime *inputDatetime_;
};

class InputDatetimeStateTrigger : public Trigger<ESPTime> {
 public:
  explicit InputDatetimeStateTrigger(InputDatetime *parent) {
    parent->add_on_state_callback([this](ESPTime value) { this->trigger(value); });
  }
};

template<typename... Ts> class InputDatetimeSetAction : public Action<Ts...> {
 public:
  InputDatetimeSetAction(InputDatetime *inputDatetime) : inputDatetime_(inputDatetime) {}
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override {
    auto call = this->inputDatetime_->make_call();

    if (this->value_.has_value()) {
      call.set_value(value_.value(x...));
    }
    call.perform();
  }

 protected:
  InputDatetime *inputDatetime_;
};

template<typename... Ts> class InputDatetimeOperationAction : public Action<Ts...> {
 public:
  explicit InputDatetimeOperationAction(InputDatetime *inputDatetime) : inputDatetime_(inputDatetime) {}
  TEMPLATABLE_VALUE(InputDatetimeOperation, operation)

  void play(Ts... x) override {
    auto call = this->inputDatetime_->make_call();
    call.with_operation(this->operation_.value(x...));
    call.perform();
  }

 protected:
  InputDatetime *inputDatetime_;
};

}  // namespace input_datetime
}  // namespace esphome
