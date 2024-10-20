#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "datetime_base.h"

namespace esphome {
namespace datetime {

#define LOG_DATETIME_DATETIME(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

class DateTimeCall;
class DateTimeEntity;

struct DateTimeEntityRestoreState {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

  DateTimeCall to_call(DateTimeEntity *datetime);
  void apply(DateTimeEntity *datetime);
} __attribute__((packed));

class DateTimeEntity : public DateTimeBase {
 protected:
  uint16_t year_;
  uint8_t month_;
  uint8_t day_;
  uint8_t hour_;
  uint8_t minute_;
  uint8_t second_;

 public:
  void publish_state();
  DateTimeCall make_call();

  ESPTime state_as_esptime() const override;

  const uint16_t &year = year_;
  const uint8_t &month = month_;
  const uint8_t &day = day_;
  const uint8_t &hour = hour_;
  const uint8_t &minute = minute_;
  const uint8_t &second = second_;

 protected:
  friend class DateTimeCall;
  friend struct DateTimeEntityRestoreState;
  friend class OnDateTimeTrigger;

  virtual void control(const DateTimeCall &call) = 0;
};

class DateTimeCall {
 public:
  explicit DateTimeCall(DateTimeEntity *parent) : parent_(parent) {}
  void perform();
  DateTimeCall &set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
  DateTimeCall &set_datetime(ESPTime datetime);
  DateTimeCall &set_datetime(const std::string &datetime);
  DateTimeCall &set_datetime(time_t epoch_seconds);

  DateTimeCall &set_year(uint16_t year) {
    this->year_ = year;
    return *this;
  }
  DateTimeCall &set_month(uint8_t month) {
    this->month_ = month;
    return *this;
  }
  DateTimeCall &set_day(uint8_t day) {
    this->day_ = day;
    return *this;
  }
  DateTimeCall &set_hour(uint8_t hour) {
    this->hour_ = hour;
    return *this;
  }
  DateTimeCall &set_minute(uint8_t minute) {
    this->minute_ = minute;
    return *this;
  }
  DateTimeCall &set_second(uint8_t second) {
    this->second_ = second;
    return *this;
  }

  optional<uint16_t> get_year() const { return this->year_; }
  optional<uint8_t> get_month() const { return this->month_; }
  optional<uint8_t> get_day() const { return this->day_; }
  optional<uint8_t> get_hour() const { return this->hour_; }
  optional<uint8_t> get_minute() const { return this->minute_; }
  optional<uint8_t> get_second() const { return this->second_; }

 protected:
  void validate_();

  DateTimeEntity *parent_;

  optional<uint16_t> year_;
  optional<uint8_t> month_;
  optional<uint8_t> day_;
  optional<uint8_t> hour_;
  optional<uint8_t> minute_;
  optional<uint8_t> second_;
};

template<typename... Ts> class DateTimeSetAction : public Action<Ts...>, public Parented<DateTimeEntity> {
 public:
  TEMPLATABLE_VALUE(ESPTime, datetime)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();

    if (this->datetime_.has_value()) {
      call.set_datetime(this->datetime_.value(x...));
    }
    call.perform();
  }
};

#ifdef USE_TIME
class OnDateTimeTrigger : public Trigger<>, public Component, public Parented<DateTimeEntity> {
 public:
  void loop() override;

 protected:
  bool matches_(const ESPTime &time) const;

  optional<ESPTime> last_check_;
};
#endif

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_DATETIME
