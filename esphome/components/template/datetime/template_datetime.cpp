#include "template_datetime.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/core/log.h"

namespace esphome::template_ {

ESPHOME_LOG_TAG(TAG, "template.datetime");

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
      // set to initial value if loading from pref failed
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
  ESPTime value = {};
  value.year = call.get_year().value_or(0);
  value.month = call.get_month().value_or(0);
  value.day_of_month = call.get_day().value_or(0);
  value.hour = call.get_hour().value_or(0);
  value.minute = call.get_minute().value_or(0);
  value.second = call.get_second().value_or(0);

  this->set_trigger_.trigger(value);

  // The fields the call sets, the current ones otherwise
  const uint16_t year = call.get_year().value_or(this->year_);
  const uint8_t month = call.get_month().value_or(this->month_);
  const uint8_t day = call.get_day().value_or(this->day_);
  const uint8_t hour = call.get_hour().value_or(this->hour_);
  const uint8_t minute = call.get_minute().value_or(this->minute_);
  const uint8_t second = call.get_second().value_or(this->second_);

  if (this->optimistic_) {
    this->year_ = year;
    this->month_ = month;
    this->day_ = day;
    this->hour_ = hour;
    this->minute_ = minute;
    this->second_ = second;
    this->publish_state();
  }

  if (this->restore_value_) {
    // Optimistic: the live state, since an on_value callback during publish_state() may have changed it
    const bool live = this->optimistic_;
    datetime::DateTimeEntityRestoreState temp = {};
    temp.year = live ? this->year_ : year;
    temp.month = live ? this->month_ : month;
    temp.day = live ? this->day_ : day;
    temp.hour = live ? this->hour_ : hour;
    temp.minute = live ? this->minute_ : minute;
    temp.second = live ? this->second_ : second;

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
