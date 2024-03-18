#include "time_entity.h"

#ifdef USE_DATETIME_TIME

#include "esphome/core/log.h"

namespace esphome {
namespace datetime {

static const char *const TAG = "datetime.time_entity";

void TimeEntity::publish_state() {
  if (this->hour_ == 0 || this->minute_ == 0 || this->second_ == 0) {
    this->has_state_ = false;
    return;
  }
  if (this->hour_ < 0 || this->hour_ > 23) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Hour must be between 0 and 23");
    return;
  }
  if (this->minute_ < 0 || this->minute_ > 59) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Minute must be between 0 and 59");
    return;
  }
  if (this->second_ < 0 || this->second_ > 59) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Second must be between 0 and 59");
    return;
  }
  this->has_state_ = true;
  ESP_LOGD(TAG, "'%s': Sending time %d:%d:%d", this->get_name().c_str(), this->hour_, this->minute_, this->second_);
  this->state_callback_.call();
}

TimeCall TimeEntity::make_call() { return TimeCall(this); }

void TimeCall::validate_() {
  if (this->hour_.has_value() && (this->hour_ < 0 || this->hour_ > 23)) {
    ESP_LOGE(TAG, "Hour must be between 0 and 23");
    this->hour_.reset();
  }
  if (this->minute_.has_value() && (this->minute_ < 0 || this->minute_ > 59)) {
    ESP_LOGE(TAG, "Minute must be between 0 and 59");
    this->minute_.reset();
  }
  if (this->second_.has_value() && (this->second_ < 0 || this->second_ > 59)) {
    ESP_LOGE(TAG, "Second must be between 0 and 59");
    this->second_.reset();
  }
}

void TimeCall::perform() {
  this->validate_();
  this->parent_->control(*this);
}

TimeCall &TimeCall::set_time(uint8_t hour, uint8_t minute, uint8_t second) {
  this->hour_ = hour;
  this->minute_ = minute;
  this->second_ = second;
  return *this;
};

TimeCall &TimeCall::set_time(ESPTime time) { return this->set_time(time.hour, time.minute, time.second); };

TimeCall &TimeCall::set_time(const std::string &time) {
  ESPTime val{};
  if (!ESPTime::strptime(time, val)) {
    ESP_LOGE(TAG, "Could not convert the time string to an ESPTime object");
    return *this;
  }
  return this->set_time(val);
}

TimeCall TimeEntityRestoreState::to_call(TimeEntity *time) {
  TimeCall call = time->make_call();
  call.set_time(this->hour, this->minute, this->second);
  return call;
}

void TimeEntityRestoreState::apply(TimeEntity *time) {
  time->hour_ = this->hour;
  time->minute_ = this->minute;
  time->second_ = this->second;
  time->publish_state();
}

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_TIME
