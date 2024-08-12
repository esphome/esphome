#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_TIME

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "datetime_base.h"

namespace esphome {
namespace datetime {

#define LOG_DATETIME_TIME(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

class TimeCall;
class TimeEntity;
class OnTimeTrigger;

struct TimeEntityRestoreState {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

  TimeCall to_call(TimeEntity *time);
  void apply(TimeEntity *time);
} __attribute__((packed));

class TimeEntity : public DateTimeBase {
 protected:
  uint8_t hour_;
  uint8_t minute_;
  uint8_t second_;

 public:
  void publish_state();
  TimeCall make_call();

  ESPTime state_as_esptime() const override {
    ESPTime obj;
    obj.hour = this->hour_;
    obj.minute = this->minute_;
    obj.second = this->second_;
    return obj;
  }

  const uint8_t &hour = hour_;
  const uint8_t &minute = minute_;
  const uint8_t &second = second_;

 protected:
  friend class TimeCall;
  friend struct TimeEntityRestoreState;
  friend class OnTimeTrigger;

  virtual void control(const TimeCall &call) = 0;
};

class TimeCall {
 public:
  explicit TimeCall(TimeEntity *parent) : parent_(parent) {}
  void perform();
  TimeCall &set_time(uint8_t hour, uint8_t minute, uint8_t second);
  TimeCall &set_time(ESPTime time);
  TimeCall &set_time(const std::string &time);

  TimeCall &set_hour(uint8_t hour) {
    this->hour_ = hour;
    return *this;
  }
  TimeCall &set_minute(uint8_t minute) {
    this->minute_ = minute;
    return *this;
  }
  TimeCall &set_second(uint8_t second) {
    this->second_ = second;
    return *this;
  }

  optional<uint8_t> get_hour() const { return this->hour_; }
  optional<uint8_t> get_minute() const { return this->minute_; }
  optional<uint8_t> get_second() const { return this->second_; }

 protected:
  void validate_();

  TimeEntity *parent_;

  optional<uint8_t> hour_;
  optional<uint8_t> minute_;
  optional<uint8_t> second_;
};

template<typename... Ts> class TimeSetAction : public Action<Ts...>, public Parented<TimeEntity> {
 public:
  TEMPLATABLE_VALUE(ESPTime, time)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();

    if (this->time_.has_value()) {
      call.set_time(this->time_.value(x...));
    }
    call.perform();
  }
};

class OnTimeTrigger : public Trigger<>, public Component, public Parented<TimeEntity> {
 public:
  void loop() override;

 protected:
  bool matches_(const ESPTime &time) const;

  optional<ESPTime> last_check_;
};

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_TIME
