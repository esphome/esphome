#include "template_datetime.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.datetime";

void TemplateDateTime::setup() {
  if (this->f_.has_value())
    return;

  ESPTime state{};

  if (!this->restore_value_) {
    state = this->initial_value_;
  } else {
    datetime::DateTimeEntityRestoreState temp;
    this->pref_ = global_preferences->make_preference<datetime::DateTimeEntityRestoreState>(194434090U ^
                                                                                            this->get_object_id_hash());
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

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->year_ = val->year;
  this->month_ = val->month;
  this->day_ = val->day_of_month;
  this->hour_ = val->hour;
  this->minute_ = val->minute;
  this->second_ = val->second;
  this->publish_state();
}

void TemplateDateTime::control(const datetime::DateTimeCall &call) {
  bool has_year = call.get_year().has_value();
  bool has_month = call.get_month().has_value();
  bool has_day = call.get_day().has_value();
  bool has_hour = call.get_hour().has_value();
  bool has_minute = call.get_minute().has_value();
  bool has_second = call.get_second().has_value();

  ESPTime value = {};
  if (has_year)
    value.year = *call.get_year();

  if (has_month)
    value.month = *call.get_month();

  if (has_day)
    value.day_of_month = *call.get_day();

  if (has_hour)
    value.hour = *call.get_hour();

  if (has_minute)
    value.minute = *call.get_minute();

  if (has_second)
    value.second = *call.get_second();

  this->set_trigger_->trigger(value);

  if (this->optimistic_) {
    if (has_year)
      this->year_ = *call.get_year();
    if (has_month)
      this->month_ = *call.get_month();
    if (has_day)
      this->day_ = *call.get_day();
    if (has_hour)
      this->hour_ = *call.get_hour();
    if (has_minute)
      this->minute_ = *call.get_minute();
    if (has_second)
      this->second_ = *call.get_second();
    this->publish_state();
  }

  if (this->restore_value_) {
    datetime::DateTimeEntityRestoreState temp = {};
    if (has_year) {
      temp.year = *call.get_year();
    } else {
      temp.year = this->year_;
    }
    if (has_month) {
      temp.month = *call.get_month();
    } else {
      temp.month = this->month_;
    }
    if (has_day) {
      temp.day = *call.get_day();
    } else {
      temp.day = this->day_;
    }
    if (has_hour) {
      temp.hour = *call.get_hour();
    } else {
      temp.hour = this->hour_;
    }
    if (has_minute) {
      temp.minute = *call.get_minute();
    } else {
      temp.minute = this->minute_;
    }
    if (has_second) {
      temp.second = *call.get_second();
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

}  // namespace template_
}  // namespace esphome

#endif  // USE_DATETIME_DATETIME
