#include "template_datetime.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.datetime";

void TemplateDateTime::setup() {
  if (this->f_.has_value())
    return;

  ESPTime state{};

  if (!this->restore_value_) {
    state = this->initial_value_;
  } else {
    datetime::DateTimeEntityRestoreState temp;
    this->pref_ = this->make_entity_preference<datetime::DateTimeEntityRestoreState>(194434090U);
    if (this->pref_.load(&temp)) {
      temp.apply(this);
      return;
    } else {
      // set to inital value if loading from pref failed
      state = this->initial_value_;
    }
  }

  this->year_ = state.year;
  this->month_ = state.month;
  this->day_ = state.day_of_month;
  this->hour_ = state.hour;
  this->minute_ = state.minute;
  this->second_ = state.second;
  this->publish_state();
}

void TemplateDateTime::update() {
  if (!this->f_.has_value())
    return;

  auto val = this->f_();
  if (val.has_value()) {
    this->year_ = val->year;
    this->month_ = val->month;
    this->day_ = val->day_of_month;
    this->hour_ = val->hour;
    this->minute_ = val->minute;
    this->second_ = val->second;
    this->publish_state();
  }
}

void TemplateDateTime::control(const datetime::DateTimeCall &call) {
  auto opt_year = call.get_year();
  auto opt_month = call.get_month();
  auto opt_day = call.get_day();
  auto opt_hour = call.get_hour();
  auto opt_minute = call.get_minute();
  auto opt_second = call.get_second();
  bool has_year = opt_year.has_value();
  bool has_month = opt_month.has_value();
  bool has_day = opt_day.has_value();
  bool has_hour = opt_hour.has_value();
  bool has_minute = opt_minute.has_value();
  bool has_second = opt_second.has_value();

  ESPTime value = {};
  if (has_year)
    value.year = *opt_year;

  if (has_month)
    value.month = *opt_month;

  if (has_day)
    value.day_of_month = *opt_day;

  if (has_hour)
    value.hour = *opt_hour;

  if (has_minute)
    value.minute = *opt_minute;

  if (has_second)
    value.second = *opt_second;

  this->set_trigger_.trigger(value);

  if (this->optimistic_) {
    if (has_year)
      this->year_ = *opt_year;
    if (has_month)
      this->month_ = *opt_month;
    if (has_day)
      this->day_ = *opt_day;
    if (has_hour)
      this->hour_ = *opt_hour;
    if (has_minute)
      this->minute_ = *opt_minute;
    if (has_second)
      this->second_ = *opt_second;
    this->publish_state();
  }

  if (this->restore_value_) {
    datetime::DateTimeEntityRestoreState temp = {};
    if (has_year) {
      temp.year = *opt_year;
    } else {
      temp.year = this->year_;
    }
    if (has_month) {
      temp.month = *opt_month;
    } else {
      temp.month = this->month_;
    }
    if (has_day) {
      temp.day = *opt_day;
    } else {
      temp.day = this->day_;
    }
    if (has_hour) {
      temp.hour = *opt_hour;
    } else {
      temp.hour = this->hour_;
    }
    if (has_minute) {
      temp.minute = *opt_minute;
    } else {
      temp.minute = this->minute_;
    }
    if (has_second) {
      temp.second = *opt_second;
    } else {
      temp.second = this->second_;
    }

    this->pref_.save(&temp);
  }
}

void TemplateDateTime::dump_config() {
  LOG_DATETIME_DATETIME("", "Template DateTime", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::template_

#endif  // USE_DATETIME_DATETIME
