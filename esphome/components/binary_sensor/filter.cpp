#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR_FILTER

#include "filter.h"

#include "binary_sensor.h"
#include "esphome/core/application.h"

namespace esphome::binary_sensor {

static const char *const TAG = "sensor.filter";

// AutorepeatFilter still inherits Component (it schedules two distinct timer
// purposes), so it keeps the (Component *, id) scheduler API.
constexpr uint32_t AUTOREPEAT_TIMING_ID = 0;
constexpr uint32_t AUTOREPEAT_ON_OFF_ID = 1;

void Filter::output(bool value) {
  if (this->next_ == nullptr) {
    this->parent_->send_state_internal(value);
  } else {
    this->next_->input(value);
  }
}
void Filter::input(bool value) {
  if (!this->dedup_.next(value))
    return;
  auto b = this->new_value(value);
  if (b.has_value()) {
    this->output(*b);
  }
}

void TimeoutFilter::input(bool value) {
  App.scheduler.set_timeout(this, this->timeout_delay_.value(), [this]() { this->parent_->invalidate_state(); });
  // we do not de-dup here otherwise changes from invalid to valid state will not be output
  this->output(value);
}

optional<bool> DelayedOnOffFilter::new_value(bool value) {
  if (value) {
    App.scheduler.set_timeout(this, this->on_delay_.value(), [this]() { this->output(true); });
  } else {
    App.scheduler.set_timeout(this, this->off_delay_.value(), [this]() { this->output(false); });
  }
  return {};
}

optional<bool> DelayedOnFilter::new_value(bool value) {
  if (value) {
    App.scheduler.set_timeout(this, this->delay_.value(), [this]() { this->output(true); });
    return {};
  } else {
    App.scheduler.cancel_timeout(this);
    return false;
  }
}

optional<bool> DelayedOffFilter::new_value(bool value) {
  if (!value) {
    App.scheduler.set_timeout(this, this->delay_.value(), [this]() { this->output(false); });
    return {};
  } else {
    App.scheduler.cancel_timeout(this);
    return true;
  }
}

optional<bool> InvertFilter::new_value(bool value) { return !value; }

// AutorepeatFilterBase
optional<bool> AutorepeatFilterBase::new_value(bool value) {
  if (value) {
    if (this->active_timing_ != 0)
      return {};
    this->next_timing_();
    return true;
  } else {
    this->cancel_timeout(AUTOREPEAT_TIMING_ID);
    this->cancel_timeout(AUTOREPEAT_ON_OFF_ID);
    this->active_timing_ = 0;
    return false;
  }
}

void AutorepeatFilterBase::next_timing_() {
  if (this->active_timing_ < this->timings_count_) {
    this->set_timeout(AUTOREPEAT_TIMING_ID, this->timings_[this->active_timing_].delay,
                      [this]() { this->next_timing_(); });
  }
  if (this->active_timing_ <= this->timings_count_) {
    this->active_timing_++;
  }
  if (this->active_timing_ == 2)
    this->next_value_(false);
}

void AutorepeatFilterBase::next_value_(bool val) {
  const AutorepeatFilterTiming &timing = this->timings_[this->active_timing_ - 2];
  this->output(val);
  this->set_timeout(AUTOREPEAT_ON_OFF_ID, val ? timing.time_on : timing.time_off,
                    [this, val]() { this->next_value_(!val); });
}

float AutorepeatFilterBase::get_setup_priority() const { return setup_priority::HARDWARE; }

LambdaFilter::LambdaFilter(std::function<optional<bool>(bool)> f) : f_(std::move(f)) {}

optional<bool> LambdaFilter::new_value(bool value) { return this->f_(value); }

optional<bool> SettleFilter::new_value(bool value) {
  if (!this->steady_) {
    App.scheduler.set_timeout(this, this->delay_.value(), [this, value]() {
      this->steady_ = true;
      this->output(value);
    });
    return {};
  } else {
    this->steady_ = false;
    App.scheduler.set_timeout(this, this->delay_.value(), [this]() { this->steady_ = true; });
    return value;
  }
}

}  // namespace esphome::binary_sensor

#endif  // USE_BINARY_SENSOR_FILTER
