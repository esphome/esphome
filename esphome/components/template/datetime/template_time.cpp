#include "template_time.h"

#ifdef USE_DATETIME_TIME

#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.time";

void TemplateTime::setup() {
  if (this->f_.has_value())
    return;

  ESPTime state{};

  if (!this->restore_value_) {
    state = this->initial_value_;
  } else {
    datetime::TimeEntityRestoreState temp;
    this->pref_ = this->make_entity_preference<datetime::TimeEntityRestoreState>(194434060U);
    if (this->pref_.load(&temp)) {
      temp.apply(this);
      return;
    } else {
      // set to inital value if loading from pref failed
      state = this->initial_value_;
    }
  }

  this->hour_ = state.hour;
  this->minute_ = state.minute;
  this->second_ = state.second;
  this->publish_state();
}

void TemplateTime::update() {
  if (!this->f_.has_value())
    return;

  auto val = this->f_();
  if (val.has_value()) {
    this->hour_ = val->hour;
    this->minute_ = val->minute;
    this->second_ = val->second;
    this->publish_state();
  }
}

void TemplateTime::control(const datetime::TimeCall &call) {
  auto opt_hour = call.get_hour();
  auto opt_minute = call.get_minute();
  auto opt_second = call.get_second();
  bool has_hour = opt_hour.has_value();
  bool has_minute = opt_minute.has_value();
  bool has_second = opt_second.has_value();

  ESPTime value = {};
  if (has_hour)
    value.hour = *opt_hour;

  if (has_minute)
    value.minute = *opt_minute;

  if (has_second)
    value.second = *opt_second;

  this->set_trigger_.trigger(value);

  if (this->optimistic_) {
    if (has_hour)
      this->hour_ = *opt_hour;
    if (has_minute)
      this->minute_ = *opt_minute;
    if (has_second)
      this->second_ = *opt_second;
    this->publish_state();
  }

  if (this->restore_value_) {
    datetime::TimeEntityRestoreState temp = {};
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

void TemplateTime::dump_config() {
  LOG_DATETIME_TIME("", "Template Time", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::template_

#endif  // USE_DATETIME_TIME
