#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_DATE

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

#include "datetime_base.h"

namespace esphome {
namespace datetime {

#define LOG_DATETIME_DATE(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

class DateCall;
class DateEntity;

struct DateEntityRestoreState {
  uint16_t year;
  uint8_t month;
  uint8_t day;

  DateCall to_call(DateEntity *date);
  void apply(DateEntity *date);
} __attribute__((packed));

class DateEntity : public DateTimeBase {
 protected:
  uint16_t year_;
  uint8_t month_;
  uint8_t day_;

 public:
  void publish_state();
  DateCall make_call();

  ESPTime state_as_esptime() const override {
    ESPTime obj;
    obj.year = this->year_;
    obj.month = this->month_;
    obj.day_of_month = this->day_;
    return obj;
  }

  const uint16_t &year = year_;
  const uint8_t &month = month_;
  const uint8_t &day = day_;

 protected:
  friend class DateCall;
  friend struct DateEntityRestoreState;

  virtual void control(const DateCall &call) = 0;
};

class DateCall {
 public:
  explicit DateCall(DateEntity *parent) : parent_(parent) {}
  void perform();
  DateCall &set_date(uint16_t year, uint8_t month, uint8_t day);
  DateCall &set_date(ESPTime time);
  DateCall &set_date(const std::string &date);

  DateCall &set_year(uint16_t year) {
    this->year_ = year;
    return *this;
  }
  DateCall &set_month(uint8_t month) {
    this->month_ = month;
    return *this;
  }
  DateCall &set_day(uint8_t day) {
    this->day_ = day;
    return *this;
  }

  optional<uint16_t> get_year() const { return this->year_; }
  optional<uint8_t> get_month() const { return this->month_; }
  optional<uint8_t> get_day() const { return this->day_; }

 protected:
  void validate_();

  DateEntity *parent_;

  optional<int16_t> year_;
  optional<uint8_t> month_;
  optional<uint8_t> day_;
};

template<typename... Ts> class DateSetAction : public Action<Ts...>, public Parented<DateEntity> {
 public:
  TEMPLATABLE_VALUE(ESPTime, date)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();

    if (this->date_.has_value()) {
      call.set_date(this->date_.value(x...));
    }
    call.perform();
  }
};

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_DATE
