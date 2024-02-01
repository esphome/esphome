#include "template_datetime.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.datetime";
static const uint8_t SZ = 20;

void TemplateDatetime::setup() {
  if (this->f_.has_value())
    return;

  std::string state{};

  if (!this->restore_value_) {
    state = this->initial_value_;

    // TODO do we need this check? initial_value_ should already be check by cv
    has_date = HAS_DATETIME_STRING_DATE_ONLY(initial_value_) || HAS_DATETIME_STRING_DATE_AND_TIME(initial_value_);
    has_time = HAS_DATETIME_STRING_TIME_ONLY(initial_value_) || HAS_DATETIME_STRING_DATE_AND_TIME(initial_value_);
    if (!(has_date || has_time)) {
      ESP_LOGE(TAG, "'%s' - Could not set initial value! Datetime string has the wrong format!",
               this->get_name().c_str());
    }
  } else {
    char temp[SZ];
    this->pref_ = global_preferences->make_preference<uint8_t[SZ]>(194434030U ^ this->get_object_id_hash());
    if (this->pref_.load(&temp)) {
      state.assign(temp + 1, temp[0]);
    } else {
      // set to inital value if loading from pref failed
      state = this->initial_value_;
    }
  }
  this->publish_state(state);
}

void TemplateDatetime::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;
  ESP_LOGD(TAG, (*val).c_str());

  std::string state = (*val).c_str();
  has_date = HAS_DATETIME_STRING_DATE_ONLY(state) || HAS_DATETIME_STRING_DATE_AND_TIME(state);
  has_time = HAS_DATETIME_STRING_TIME_ONLY(state) || HAS_DATETIME_STRING_DATE_AND_TIME(state);
  if (!(has_date || has_time)) {
    // ESP_LOGE(TAG, "'%s' - Could not update value! Datetime string has the wrong format!", this->get_name().c_str());
    // return;
  }
  this->publish_state(state);
}

void TemplateDatetime::control(std::string value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_) {
    unsigned char temp[SZ];
    int size = this->state.size();
    memcpy(temp + 1, this->state.c_str(), size);
    temp[0] = ((unsigned char) size);
    this->pref_.save(&temp);
  }
}

void TemplateDatetime::dump_config() {
  LOG_DATETIME("", "Template Input_Datetime", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome
