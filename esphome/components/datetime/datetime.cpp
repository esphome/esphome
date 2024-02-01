#include "datetime.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace datetime {

static const char *const TAG = "datetime";

Datetime::Datetime() {}

void Datetime::publish_state(std::string state) {
  this->has_state_ = true;
  this->state = state;
  this->has_date = HAS_DATETIME_STRING_DATE_ONLY(state) || HAS_DATETIME_STRING_DATE_AND_TIME(state);
  this->has_time = HAS_DATETIME_STRING_TIME_ONLY(state) || HAS_DATETIME_STRING_DATE_AND_TIME(state);

  if (!ESPTime::strptime(state, this->state_as_time)) {
    ESP_LOGE(TAG, "'%s' Could not convert the dateime string to an ESPTime objekt", this->get_name().c_str());
  }
  this->state_as_time.day_of_year = 0;
  this->state_as_time.day_of_week = 1;  // not important for us, but has to be set to get a utc timestamp

  const uint8_t DAYS_IN_MONTH[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (int i = 1; i < this->state_as_time.month; i++) {
    this->state_as_time.day_of_year += DAYS_IN_MONTH[i];
  }

  this->state_as_time.day_of_year += this->state_as_time.day_of_month;

  ESP_LOGD(TAG, "'%s': Sending state %s", this->get_name().c_str(), this->state.c_str());
  this->state_callback_.call(state);
}

void Datetime::add_on_state_callback(std::function<void(std::string)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace datetime
}  // namespace esphome
