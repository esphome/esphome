#include "template_date.h"

#ifdef USE_DATETIME_DATE

#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.date";

void TemplateDate::setup() {
  if (this->f_.has_value())
    return;

  ESPTime state{};

  if (!this->restore_value_) {
    state = this->initial_value_;
  } else {
    datetime::DateEntityRestoreState temp;
    this->pref_ =
        global_preferences->make_preference<datetime::DateEntityRestoreState>(194434030U ^ this->get_object_id_hash());
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

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->year_ = val->year;
  this->month_ = val->month;
  this->day_ = val->day_of_month;
  this->publish_state();
}

void TemplateDate::control(const datetime::DateCall &call) {
  bool has_year = call.get_year().has_value();
  bool has_month = call.get_month().has_value();
  bool has_day = call.get_day().has_value();

  ESPTime value = {};
  if (has_year)
    value.year = *call.get_year();

  if (has_month)
    value.month = *call.get_month();

  if (has_day)
    value.day_of_month = *call.get_day();

  this->set_trigger_->trigger(value);

  if (this->optimistic_) {
    if (has_year)
      this->year_ = *call.get_year();
    if (has_month)
      this->month_ = *call.get_month();
    if (has_day)
      this->day_ = *call.get_day();
    this->publish_state();
  }

  if (this->restore_value_) {
    datetime::DateEntityRestoreState temp = {};
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

    this->pref_.save(&temp);
  }
}

void TemplateDate::dump_config() {
  LOG_DATETIME_DATE("", "Template Date", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

#endif  // USE_DATETIME_DATE
