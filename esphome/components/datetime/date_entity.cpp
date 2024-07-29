#include "date_entity.h"

#ifdef USE_DATETIME_DATE

#include "esphome/core/log.h"

namespace esphome {
namespace datetime {

static const char *const TAG = "datetime.date_entity";

void DateEntity::publish_state() {
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
  this->has_state_ = true;
  ESP_LOGD(TAG, "'%s': Sending date %d-%d-%d", this->get_name().c_str(), this->year_, this->month_, this->day_);
  this->state_callback_.call();
}

DateCall DateEntity::make_call() { return DateCall(this); }

void DateCall::validate_() {
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
}

void DateCall::perform() {
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
  this->parent_->control(*this);
}

DateCall &DateCall::set_date(uint16_t year, uint8_t month, uint8_t day) {
  this->year_ = year;
  this->month_ = month;
  this->day_ = day;
  return *this;
};

DateCall &DateCall::set_date(ESPTime time) { return this->set_date(time.year, time.month, time.day_of_month); };

DateCall &DateCall::set_date(const std::string &date) {
  ESPTime val{};
  if (!ESPTime::strptime(date, val)) {
    ESP_LOGE(TAG, "Could not convert the date string to an ESPTime object");
    return *this;
  }
  return this->set_date(val);
}

DateCall DateEntityRestoreState::to_call(DateEntity *date) {
  DateCall call = date->make_call();
  call.set_date(this->year, this->month, this->day);
  return call;
}

void DateEntityRestoreState::apply(DateEntity *date) {
  date->year_ = this->year;
  date->month_ = this->month;
  date->day_ = this->day;
  date->publish_state();
}

}  // namespace datetime
}  // namespace esphome

#endif  // USE_DATETIME_DATE
