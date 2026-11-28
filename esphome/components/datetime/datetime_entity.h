#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "datetime_base.h"

namespace esphome::datetime {

#define LOG_DATETIME_DATETIME(prefix, type, obj) \
  if (log_entity_header(TAG, prefix, LOG_STR_LITERAL(type), (obj))) { \
    LOG_ENTITY_ICON(TAG, prefix, *(obj)); \
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
  friend class OnDateTimeBase;

  virtual void control(const DateTimeCall &call) = 0;
};

class DateTimeCall {
 public:
  explicit DateTimeCall(DateTimeEntity *parent) : parent_(parent) {}
  void perform();
  DateTimeCall &set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
  DateTimeCall &set_datetime(ESPTime datetime);
  DateTimeCall &set_datetime(const char *datetime, size_t len);
  DateTimeCall &set_datetime(const char *datetime) { return this->set_datetime(datetime, strlen(datetime)); }
  DateTimeCall &set_datetime(const std::string &datetime) {
    return this->set_datetime(datetime.c_str(), datetime.size());
  }
  DateTimeCall &set_datetime(time_t epoch_seconds);

  DateTimeCall &set_year(uint16_t year) {
    this->year_ = year;
    this->set_ |= YEAR_SET;
    return *this;
  }
  DateTimeCall &set_month(uint8_t month) {
    this->month_ = month;
    this->set_ |= MONTH_SET;
    return *this;
  }
  DateTimeCall &set_day(uint8_t day) {
    this->day_ = day;
    this->set_ |= DAY_SET;
    return *this;
  }
  DateTimeCall &set_hour(uint8_t hour) {
    this->hour_ = hour;
    this->set_ |= HOUR_SET;
    return *this;
  }
  DateTimeCall &set_minute(uint8_t minute) {
    this->minute_ = minute;
    this->set_ |= MINUTE_SET;
    return *this;
  }
  DateTimeCall &set_second(uint8_t second) {
    this->second_ = second;
    this->set_ |= SECOND_SET;
    return *this;
  }

  optional<uint16_t> get_year() const { return this->field_(YEAR_SET, this->year_); }
  optional<uint8_t> get_month() const { return this->field_(MONTH_SET, this->month_); }
  optional<uint8_t> get_day() const { return this->field_(DAY_SET, this->day_); }
  optional<uint8_t> get_hour() const { return this->field_(HOUR_SET, this->hour_); }
  optional<uint8_t> get_minute() const { return this->field_(MINUTE_SET, this->minute_); }
  optional<uint8_t> get_second() const { return this->field_(SECOND_SET, this->second_); }

 protected:
  // One bit per field in set_ instead of an optional<> each, so the call fits the scheduler's 12-byte inline slot
  static constexpr uint8_t YEAR_SET = 1 << 0;
  static constexpr uint8_t MONTH_SET = 1 << 1;
  static constexpr uint8_t DAY_SET = 1 << 2;
  static constexpr uint8_t HOUR_SET = 1 << 3;
  static constexpr uint8_t MINUTE_SET = 1 << 4;
  static constexpr uint8_t SECOND_SET = 1 << 5;
  bool has_(uint8_t bit) const { return (this->set_ & bit) != 0; }
  template<typename T> optional<T> field_(uint8_t bit, T value) const {
    return this->has_(bit) ? optional<T>{value} : optional<T>{};
  }

  void validate_();

  DateTimeEntity *parent_;

  uint16_t year_{0};
  uint8_t month_{0};
  uint8_t day_{0};
  uint8_t hour_{0};
  uint8_t minute_{0};
  uint8_t second_{0};
  uint8_t set_{0};
};
static_assert(sizeof(void *) != 4 || sizeof(DateTimeCall) <= 12, "DateTimeCall must stay within 12 bytes on 32-bit");

inline DateTimeCall DateTimeEntity::make_call() { return DateTimeCall(this); }

#ifdef USE_TIME
/// Runs fire() when the clock reaches the entity's date and time, for on_time.
class OnDateTimeBase : public FiringComponent<>, public Parented<DateTimeEntity> {
 public:
  explicit OnDateTimeBase(DateTimeEntity *parent) : Parented(parent) {}
  void loop() override;

 protected:
  bool matches_(const ESPTime &time) const;

  optional<ESPTime> last_check_;
};
#endif

}  // namespace esphome::datetime

#endif  // USE_DATETIME_DATETIME
