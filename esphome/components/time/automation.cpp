#include "automation.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace time {

static const char *const TAG = "automation";
static const int MAX_TIMESTAMP_DRIFT = 900;  // how far can the clock drift before we consider
                                             // there has been a drastic time synchronization

void CronTrigger::add_second(uint8_t second) { this->seconds_[second] = true; }
void CronTrigger::add_minute(uint8_t minute) { this->minutes_[minute] = true; }
void CronTrigger::add_hour(uint8_t hour) { this->hours_[hour] = true; }
void CronTrigger::add_day_of_month(uint8_t day_of_month) { this->days_of_month_[day_of_month] = true; }
void CronTrigger::add_month(uint8_t month) { this->months_[month] = true; }
void CronTrigger::add_day_of_week(uint8_t day_of_week) { this->days_of_week_[day_of_week] = true; }
bool CronTrigger::matches(const ESPTime &time) {
  return time.is_valid() && this->seconds_[time.second] && this->minutes_[time.minute] && this->hours_[time.hour] &&
         this->days_of_month_[time.day_of_month] && this->months_[time.month] && this->days_of_week_[time.day_of_week];
}
void CronTrigger::loop() {
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
    ESP_LOGD(TAG, "Second=%02u Minute=%02u Hour=%02u DayOfWeek=%u DayOfMonth=%u DayOfYear=%u Month=%u time=%" PRId64,
             time.second, time.minute, time.hour, time.day_of_week, time.day_of_month, time.day_of_year, time.month,
             (int64_t) time.timestamp);
  }

  if (this->matches(time))
    this->trigger();
}
CronTrigger::CronTrigger(RealTimeClock *rtc) : rtc_(rtc) {}
void CronTrigger::add_seconds(const std::vector<uint8_t> &seconds) {
  for (uint8_t it : seconds)
    this->add_second(it);
}
void CronTrigger::add_minutes(const std::vector<uint8_t> &minutes) {
  for (uint8_t it : minutes)
    this->add_minute(it);
}
void CronTrigger::add_hours(const std::vector<uint8_t> &hours) {
  for (uint8_t it : hours)
    this->add_hour(it);
}
void CronTrigger::add_days_of_month(const std::vector<uint8_t> &days_of_month) {
  for (uint8_t it : days_of_month)
    this->add_day_of_month(it);
}
void CronTrigger::add_months(const std::vector<uint8_t> &months) {
  for (uint8_t it : months)
    this->add_month(it);
}
void CronTrigger::add_days_of_week(const std::vector<uint8_t> &days_of_week) {
  for (uint8_t it : days_of_week)
    this->add_day_of_week(it);
}
float CronTrigger::get_setup_priority() const { return setup_priority::HARDWARE; }

SyncTrigger::SyncTrigger(RealTimeClock *rtc) : rtc_(rtc) {
  rtc->add_on_time_sync_callback([this]() { this->trigger(); });
}

}  // namespace time
}  // namespace esphome
