#include "template_time.h"

#ifdef USE_DATETIME_TIME

#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.time";

void TemplateTime::setup() {
  if (this->f_.has_value())
    return;

  ESPTime state{};

  if (!this->restore_value_) {
    state = this->initial_value_;
  } else {
    datetime::TimeEntityRestoreState temp;
    this->pref_ =
        global_preferences->make_preference<datetime::TimeEntityRestoreState>(194434060U ^ this->get_object_id_hash());
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

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->hour_ = val->hour;
  this->minute_ = val->minute;
  this->second_ = val->second;
  this->publish_state();
}

void TemplateTime::control(const datetime::TimeCall &call) {
  bool has_hour = call.get_hour().has_value();
  bool has_minute = call.get_minute().has_value();
  bool has_second = call.get_second().has_value();

  ESPTime value = {};
  if (has_hour)
    value.hour = *call.get_hour();

  if (has_minute)
    value.minute = *call.get_minute();

  if (has_second)
    value.second = *call.get_second();

  this->set_trigger_->trigger(value);

  if (this->optimistic_) {
    if (has_hour)
      this->hour_ = *call.get_hour();
    if (has_minute)
      this->minute_ = *call.get_minute();
    if (has_second)
      this->second_ = *call.get_second();
    this->publish_state();
  }

  if (this->restore_value_) {
    datetime::TimeEntityRestoreState temp = {};
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

void TemplateTime::dump_config() {
  LOG_DATETIME_TIME("", "Template Time", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

#endif  // USE_DATETIME_TIME
