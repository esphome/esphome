#include "template_input_datetime.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.input_datetime";

void TemplateInputDatetime::setup() {
  if (this->f_.has_value())
    return;

  TemplateInputDatetimeRTCValue recovered{};
  if (!this->restore_value_) {
    ESP_LOGD("maydebug", "!restore_value_");
    recovered.value = this->initial_value_;
    recovered.has_date = this->has_date;
    recovered.has_time = this->has_time;
  } else {
    this->pref_ = global_preferences->make_preference<TemplateInputDatetimeRTCValue>(this->get_object_id_hash());
    if (!this->pref_.load(&recovered)) {
      ESP_LOGD("maydebug", "restore!");

      recovered.value = this->initial_value_;
      recovered.has_date = this->has_date;
      recovered.has_time = this->has_time;
    }
  }
  ESP_LOGD("mydebug", recovered.value.strftime("%F %T").c_str());
  this->publish_state(recovered.value);
}

void TemplateInputDatetime::update() {
  ESP_LOGD(TAG, "Update");
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;
  ESPTime time{};
  ESP_LOGD(TAG, (*val).c_str());

  std::string value = (*val).c_str();

  if (!ESPTime::strptime(value, time)) {
    ESP_LOGD(
        TAG,
        "Parsing the datetime sting failed, correct formats are - '2020-08-25 05:30:00', '2020-08-25', '05:30:00' - "
        "your string was - %s",
        (*val).c_str());
    return;
  }
  // ESP_LOGD(TAG, std::to_string(time.year).c_str());
  // ESP_LOGD(TAG, std::to_string(time.month).c_str());
  // ESP_LOGD(TAG, std::to_string(time.day_of_month).c_str());
  // ESP_LOGD(TAG, std::to_string(time.hour).c_str());
  // ESP_LOGD(TAG, std::to_string(time.minute).c_str());
  // ESP_LOGD(TAG, std::to_string(time.second).c_str());
  // ESP_LOGD(TAG, "befor publish");

  this->publish_state(time);
}

void TemplateInputDatetime::control(ESPTime value) {
  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_) {
    TemplateInputDatetimeRTCValue save{};
    save.value = value;
    save.has_date = has_date;
    save.has_time = has_time;

    this->pref_.save(&save);
  }
}

void TemplateInputDatetime::dump_config() {
  LOG_INPUT_DATETIME("", "Template Input_Datetime", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome
