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

  ESP_LOGD("maydebug", "loading initial value");
  state = this->initial_value_;
  has_date = HAS_DATETIME_STRING_DATE_ONLY(initial_value_) || HAS_DATETIME_STRING_DATE_AND_TIME(initial_value_);
  has_time = HAS_DATETIME_STRING_TIME_ONLY(initial_value_) || HAS_DATETIME_STRING_DATE_AND_TIME(initial_value_);
  if (!(has_date || has_time)) {
    ESP_LOGE(TAG, "'%s' - Could not set initial value! Datetime string has the wrong format!",
             this->get_name().c_str());
  }

  if (this->restore_value_) {
    char temp[SZ];
    this->pref_ = global_preferences->make_preference<uint8_t[SZ]>(194434030U ^ this->get_object_id_hash());
    if (this->pref_.load(&temp)) {
      state.assign(temp + 1, temp[SZ]);
    } else {
      ESP_LOGE(TAG, "'%s' - Could not load stored value!", this->get_name().c_str());
    }

    // this->pref_ = global_preferences->make_preference<TemplateDatetimeRTCValue>(this->get_object_id_hash());
    // if (!this->pref_.load(&recovered)) {
    //   ESP_LOGD("maydebug", "loading saved value");

    //   // recovered.value = this->initial_value_;
    //   recovered.value = ESPTime{0};  // fix this!!! parse time from string
    //   recovered.has_date = this->has_date;
    //   recovered.has_time = this->has_time;
  }
  this->publish_state(state);  // fix me!!!!!!!!!!!!!!
}

void TemplateDatetime::update() {
  ESP_LOGD(TAG, "Update");
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
    ESP_LOGE(TAG, "'%s' - Could not update value! Datetime string has the wrong format!", this->get_name().c_str());
    return;
  }
  this->publish_state(state);
}

void TemplateDatetime::control(std::string value) {
  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_) {
    ESP_LOGE("mydebug", "saving pref");

    unsigned char temp[SZ];
    int size = this->state.size();
    memcpy(temp + 1, this->state.c_str(), size);
    // SZ should be pre checked at the schema level, it can't go past the char range.
    temp[0] = ((unsigned char) size);
    this->pref_.save(&temp);
    this->pref_.save(this->state.c_str());
  }
}

void TemplateDatetime::dump_config() {
  LOG_DATETIME("", "Template Input_Datetime", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome
