#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/time.h"
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome::datetime {

class DateTimeBase : public EntityBase {
 public:
  virtual ESPTime state_as_esptime() const = 0;

  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

#ifdef USE_TIME
  void set_rtc(time::RealTimeClock *rtc) { this->rtc_ = rtc; }
  time::RealTimeClock *get_rtc() const { return this->rtc_; }
#endif

 protected:
  LazyCallbackManager<void()> state_callback_;

#ifdef USE_TIME
  time::RealTimeClock *rtc_;
#endif
};

class DateTimeStateTrigger : public Trigger<ESPTime> {
 public:
  explicit DateTimeStateTrigger(DateTimeBase *parent) : parent_(parent) {
    parent->add_on_state_callback([this]() { this->trigger(this->parent_->state_as_esptime()); });
  }

 protected:
  DateTimeBase *parent_;
};

}  // namespace esphome::datetime
