#pragma once

#include "esphome/components/datetime/datetime.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace datetime {

static const char *const TAG = "input_datetime.automation";
#ifdef USE_TIME
class DatetimeOnTimeTrigger : public Trigger<>, public Component {
 public:
  explicit DatetimeOnTimeTrigger(Datetime *Datetime, time::RealTimeClock *rtc) : Datetime_(Datetime), rtc_(rtc) {}

  bool matches(const ESPTime &time);

  void loop() override;
  float get_setup_priority() const override;

 protected:
  time::RealTimeClock *rtc_;
  optional<ESPTime> last_check_;
  Datetime *Datetime_;
};
#endif  // USE_TIME

class DatetimeValueTrigger : public Trigger<std::string> {
 public:
  explicit DatetimeValueTrigger(Datetime *parent) {
    parent->add_on_state_callback([this](std::string value) { this->trigger(value); });
  }
};

template<typename... Ts> class DatetimeSetAction : public Action<Ts...> {
 public:
  DatetimeSetAction(Datetime *Datetime) : Datetime_(Datetime) {}
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override {
    auto call = this->Datetime_->make_call();

    if (this->value_.has_value()) {
      call.set_value(value_.value(x...));
    }
    call.perform();
  }

 protected:
  Datetime *Datetime_;
};

template<typename... Ts> class DatetimeOperationAction : public Action<Ts...> {
 public:
  explicit DatetimeOperationAction(Datetime *Datetime) : Datetime_(Datetime) {}
  TEMPLATABLE_VALUE(DatetimeOperation, operation)

  void play(Ts... x) override {
    auto call = this->Datetime_->make_call();
    call.with_operation(this->operation_.value(x...));
    call.perform();
  }

 protected:
  Datetime *Datetime_;
};

template<typename... Ts> class DatetimeHasDateCondition : public Condition<Ts...> {
 public:
  DatetimeHasDateCondition(Datetime *parent) : parent_(parent) {}

  bool check(Ts... x) override {
    const std::string state = this->parent_->state;
    return HAS_DATETIME_STRING_DATE_AND_TIME(state) || HAS_DATETIME_STRING_DATE_ONLY(state);
  }

 protected:
  Datetime *parent_;
};

template<typename... Ts> class DatetimeHasTimeCondition : public Condition<Ts...> {
 public:
  DatetimeHasTimeCondition(Datetime *parent) : parent_(parent) {}

  bool check(Ts... x) override {
    const std::string state = this->parent_->state;
    return HAS_DATETIME_STRING_DATE_AND_TIME(state) || HAS_DATETIME_STRING_TIME_ONLY(state);
  }

 protected:
  Datetime *parent_;
};

}  // namespace datetime
}  // namespace esphome
