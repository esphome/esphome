#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/time.h"
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace datetime {

class DateTimeBase : public EntityBase {
 public:
  /// Return whether this Datetime has gotten a full state yet.
  bool has_state() const { return this->has_state_; }

  virtual ESPTime state_as_esptime() const = 0;

  void add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }

#ifdef USE_TIME
  void set_rtc(time::RealTimeClock *rtc) { this->rtc_ = rtc; }
  time::RealTimeClock *get_rtc() const { return this->rtc_; }
#endif

 protected:
  CallbackManager<void()> state_callback_;

#ifdef USE_TIME
  time::RealTimeClock *rtc_;
#endif

  bool has_state_{false};
};

#ifdef USE_TIME
class DateTimeStateTrigger : public Trigger<ESPTime> {
 public:
  explicit DateTimeStateTrigger(DateTimeBase *parent) {
    parent->add_on_state_callback([this, parent]() { this->trigger(parent->state_as_esptime()); });
  }
};
#endif

}  // namespace datetime
}  // namespace esphome
