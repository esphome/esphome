#include "time_entity.h"

#ifdef USE_DATETIME_TIME

#include "esphome/core/log.h"

namespace esphome {
namespace datetime {

static const char *const TAG = "datetime.time_entity";

void TimeEntity::publish_state() {
  if (this->hour_ > 23) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Hour must be between 0 and 23");
    return;
  }
  if (this->minute_ > 59) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Minute must be between 0 and 59");
    return;
  }
  if (this->second_ > 59) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Second must be between 0 and 59");
    return;
  }
  this->has_state_ = true;
  ESP_LOGD(TAG, "'%s': Sending time %02d:%02d:%02d", this->get_name().c_str(), this->hour_, this->minute_,
           this->second_);
  this->state_callback_.call();
}

TimeCall TimeEntity::make_call() { return TimeCall(this); }

void TimeCall::validate_() {
  if (this->hour_.has_value() && this->hour_ > 23) {
    ESP_LOGE(TAG, "Hour must be between 0 and 23");
    this->hour_.reset();
  }
  if (this->minute_.has_value() && this->minute_ > 59) {
    ESP_LOGE(TAG, "Minute must be between 0 and 59");
    this->minute_.reset();
  }
  if (this->second_.has_value() && this->second_ > 59) {
    ESP_LOGE(TAG, "Second must be between 0 and 59");
    this->second_.reset();
  }
}

void TimeCall::perform() {
  this->validate_();
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  if (this->hour_.has_value()) {
    ESP_LOGD(TAG, " Hour: %d", *this->hour_);
  }
  if (this->minute_.has_value()) {
    ESP_LOGD(TAG, " Minute: %d", *this->minute_);
  }
  if (this->second_.has_value()) {
    ESP_LOGD(TAG, " Second: %d", *this->second_);
  }
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

#ifdef USE_TIME
static const int MAX_TIMESTAMP_DRIFT = 900;  // how far can the clock drift before we consider
                                             // there has been a drastic time synchronization

void OnTimeTrigger::loop() {
  if (!this->parent_->has_state()) {
    return;
  }
  ESPTime time = this->parent_->rtc_->now();
  if (!time.is_valid()) {
    return;
  }
  if (this->last_check_.has_value()) {
    if (*this->last_check_ > time && this->last_check_->timestamp - time.timestamp > MAX_TIMESTAMP_DRIFT) {
      // We went back in time (a lot), probably caused by time synchronization
      ESP_LOGW(TAG, "Time has jumped back!");
    } else if (*this->last_check_ >= time) {
      // already handled this one
      return;
    } else if (time > *this->last_check_ && time.timestamp - this->last_check_->timestamp > MAX_TIMESTAMP_DRIFT) {
      // We went ahead in time (a lot), probably caused by time synchronization
      ESP_LOGW(TAG, "Time has jumped ahead!");
      this->last_check_ = time;
      return;
    }

    while (true) {
      this->last_check_->increment_second();
      if (*this->last_check_ >= time)
        break;

      if (this->matches_(*this->last_check_)) {
        this->trigger();
        break;
      }
    }
  }

  this->last_check_ = time;
  if (!time.fields_in_range()) {
    ESP_LOGW(TAG, "Time is out of range!");
    ESP_LOGD(TAG, "Second=%02u Minute=%02u Hour=%02u", time.second, time.minute, time.hour);
  }

  if (this->matches_(time))
    this->trigger();
}

bool OnTimeTrigger::matches_(const ESPTime &time) const {
  return time.is_valid() && time.hour == this->parent_->hour && time.minute == this->parent_->minute &&
         time.second == this->parent_->second;
}
#endif

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_TIME
