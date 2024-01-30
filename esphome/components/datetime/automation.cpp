#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace datetime {

#ifdef USE_TIME
static const int MAX_TIMESTAMP_DRIFT = 900;  // how far can the clock drift before we consider
                                             // there has been a drastic time synchronization

bool DatetimeOnTimeTrigger::matches(const ESPTime &time) {
  if (!time.is_valid() || (!this->Datetime_->has_date && !this->Datetime_->has_time))
    return false;

  if (!this->Datetime_->has_date && time.hour == this->Datetime_->state_as_time.hour &&
      time.minute == this->Datetime_->state_as_time.minute && time.second == this->Datetime_->state_as_time.second) {
    return true;
  }

  if (!this->Datetime_->has_time && time.year == this->Datetime_->state_as_time.year &&
      time.day_of_month == this->Datetime_->state_as_time.day_of_month &&
      time.month == this->Datetime_->state_as_time.month) {
    return true;
  }

  if (this->Datetime_->has_time && this->Datetime_->has_date && time.year == this->Datetime_->state_as_time.year &&
      time.day_of_month == this->Datetime_->state_as_time.day_of_month &&
      time.month == this->Datetime_->state_as_time.month && time.hour == this->Datetime_->state_as_time.hour &&
      time.minute == this->Datetime_->state_as_time.minute && time.second == this->Datetime_->state_as_time.second) {
    return true;
  }

  return false;
}

void DatetimeOnTimeTrigger::loop() {
  ESPTime time = this->rtc_->now();
  if (!time.is_valid())
    return;

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

      if (this->matches(*this->last_check_))
        this->trigger();
    }
  }

  this->last_check_ = time;
  if (!time.fields_in_range()) {
    ESP_LOGW(TAG, "Time is out of range!");
    ESP_LOGD(TAG, "Second=%02u Minute=%02u Hour=%02u DayOfWeek=%u DayOfMonth=%u DayOfYear=%u Month=%u time=%",
             time.second, time.minute, time.hour, time.day_of_week, time.day_of_month, time.day_of_year, time.month,
             (int64_t) time.timestamp);
  }

  if (this->matches(time))
    this->trigger();
}

float DatetimeOnTimeTrigger::get_setup_priority() const { return setup_priority::HARDWARE; }

#endif  // USE_TIME

}  // namespace datetime
}  // namespace esphome
