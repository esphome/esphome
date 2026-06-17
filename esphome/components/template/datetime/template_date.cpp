#include "template_date.h"

#ifdef USE_DATETIME_DATE

#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.date";

void TemplateDate::setup() {
  if (this->f_.has_value())
    return;

  ESPTime state{};

  if (!this->restore_value_) {
    state = this->initial_value_;
  } else {
    datetime::DateEntityRestoreState temp;
    this->pref_ = this->make_entity_preference<datetime::DateEntityRestoreState>(194434030U);
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
  this->publish_state();
}

void TemplateDate::update() {
  if (!this->f_.has_value())
    return;

  auto val = this->f_();
  if (val.has_value()) {
    this->year_ = val->year;
    this->month_ = val->month;
    this->day_ = val->day_of_month;
    this->publish_state();
  }
}

void TemplateDate::control(const datetime::DateCall &call) {
  auto opt_year = call.get_year();
  auto opt_month = call.get_month();
  auto opt_day = call.get_day();
  bool has_year = opt_year.has_value();
  bool has_month = opt_month.has_value();
  bool has_day = opt_day.has_value();

  ESPTime value = {};
  if (has_year)
    value.year = *opt_year;

  if (has_month)
    value.month = *opt_month;

  if (has_day)
    value.day_of_month = *opt_day;

  this->set_trigger_.trigger(value);

  if (this->optimistic_) {
    if (has_year)
      this->year_ = *opt_year;
    if (has_month)
      this->month_ = *opt_month;
    if (has_day)
      this->day_ = *opt_day;
    this->publish_state();
  }

  if (this->restore_value_) {
    datetime::DateEntityRestoreState temp = {};
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

    this->pref_.save(&temp);
  }
}

void TemplateDate::dump_config() {
  LOG_DATETIME_DATE("", "Template Date", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::template_

#endif  // USE_DATETIME_DATE
