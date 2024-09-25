#include "datetime_entity.h"

#ifdef USE_DATETIME_DATETIME

#include "esphome/core/log.h"

namespace esphome {
namespace datetime {

static const char *const TAG = "datetime.datetime_entity";

void DateTimeEntity::publish_state() {
  if (this->year_ == 0 || this->month_ == 0 || this->day_ == 0) {
    this->has_state_ = false;
    return;
  }
  if (this->year_ < 1970 || this->year_ > 3000) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Year must be between 1970 and 3000");
    return;
  }
  if (this->month_ < 1 || this->month_ > 12) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Month must be between 1 and 12");
    return;
  }
  if (this->day_ > days_in_month(this->month_, this->year_)) {
    this->has_state_ = false;
    ESP_LOGE(TAG, "Day must be between 1 and %d for month %d", days_in_month(this->month_, this->year_), this->month_);
    return;
  }
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
  ESP_LOGD(TAG, "'%s': Sending datetime %04u-%02u-%02u %02d:%02d:%02d", this->get_name().c_str(), this->year_,
           this->month_, this->day_, this->hour_, this->minute_, this->second_);
  this->state_callback_.call();
}

DateTimeCall DateTimeEntity::make_call() { return DateTimeCall(this); }

ESPTime DateTimeEntity::state_as_esptime() const {
  ESPTime obj;
  obj.year = this->year_;
  obj.month = this->month_;
  obj.day_of_month = this->day_;
  obj.hour = this->hour_;
  obj.minute = this->minute_;
  obj.second = this->second_;
  obj.day_of_week = 1;  // Required to be valid for recalc_timestamp_local but not used.
  obj.day_of_year = 1;  // Required to be valid for recalc_timestamp_local but not used.
  obj.recalc_timestamp_local(false);
  return obj;
}

void DateTimeCall::validate_() {
  if (this->year_.has_value() && (this->year_ < 1970 || this->year_ > 3000)) {
    ESP_LOGE(TAG, "Year must be between 1970 and 3000");
    this->year_.reset();
    this->month_.reset();
    this->day_.reset();
  }
  if (this->month_.has_value() && (this->month_ < 1 || this->month_ > 12)) {
    ESP_LOGE(TAG, "Month must be between 1 and 12");
    this->month_.reset();
    this->day_.reset();
  }
  if (this->day_.has_value()) {
    uint16_t year = 0;
    uint8_t month = 0;
    if (this->month_.has_value()) {
      month = *this->month_;
    } else {
      if (this->parent_->month != 0) {
        month = this->parent_->month;
      } else {
        ESP_LOGE(TAG, "Month must be set to validate day");
        this->day_.reset();
      }
    }
    if (this->year_.has_value()) {
      year = *this->year_;
    } else {
      if (this->parent_->year != 0) {
        year = this->parent_->year;
      } else {
        ESP_LOGE(TAG, "Year must be set to validate day");
        this->day_.reset();
      }
    }
    if (this->day_.has_value() && *this->day_ > days_in_month(month, year)) {
      ESP_LOGE(TAG, "Day must be between 1 and %d for month %d", days_in_month(month, year), month);
      this->day_.reset();
    }
  }

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

void DateTimeCall::perform() {
  this->validate_();
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());

  if (this->year_.has_value()) {
    ESP_LOGD(TAG, " Year: %d", *this->year_);
  }
  if (this->month_.has_value()) {
    ESP_LOGD(TAG, " Month: %d", *this->month_);
  }
  if (this->day_.has_value()) {
    ESP_LOGD(TAG, " Day: %d", *this->day_);
  }
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

DateTimeCall &DateTimeCall::set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                                         uint8_t second) {
  this->year_ = year;
  this->month_ = month;
  this->day_ = day;
  this->hour_ = hour;
  this->minute_ = minute;
  this->second_ = second;
  return *this;
};

DateTimeCall &DateTimeCall::set_datetime(ESPTime datetime) {
  return this->set_datetime(datetime.year, datetime.month, datetime.day_of_month, datetime.hour, datetime.minute,
                            datetime.second);
};

DateTimeCall &DateTimeCall::set_datetime(const std::string &datetime) {
  ESPTime val{};
  if (!ESPTime::strptime(datetime, val)) {
    ESP_LOGE(TAG, "Could not convert the time string to an ESPTime object");
    return *this;
  }
  return this->set_datetime(val);
}

DateTimeCall &DateTimeCall::set_datetime(time_t epoch_seconds) {
  ESPTime val = ESPTime::from_epoch_local(epoch_seconds);
  return this->set_datetime(val);
}

DateTimeCall DateTimeEntityRestoreState::to_call(DateTimeEntity *datetime) {
  DateTimeCall call = datetime->make_call();
  call.set_datetime(this->year, this->month, this->day, this->hour, this->minute, this->second);
  return call;
}

void DateTimeEntityRestoreState::apply(DateTimeEntity *time) {
  time->year_ = this->year;
  time->month_ = this->month;
  time->day_ = this->day;
  time->hour_ = this->hour;
  time->minute_ = this->minute;
  time->second_ = this->second;
  time->publish_state();
}

#ifdef USE_TIME
static const int MAX_TIMESTAMP_DRIFT = 900;  // how far can the clock drift before we consider
                                             // there has been a drastic time synchronization

void OnDateTimeTrigger::loop() {
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
    ESP_LOGD(TAG, "Second=%02u Minute=%02u Hour=%02u Day=%02u Month=%02u Year=%04u", time.second, time.minute,
             time.hour, time.day_of_month, time.month, time.year);
  }

  if (this->matches_(time))
    this->trigger();
}

bool OnDateTimeTrigger::matches_(const ESPTime &time) const {
  return time.is_valid() && time.year == this->parent_->year && time.month == this->parent_->month &&
         time.day_of_month == this->parent_->day && time.hour == this->parent_->hour &&
         time.minute == this->parent_->minute && time.second == this->parent_->second;
}
#endif

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_TIME
