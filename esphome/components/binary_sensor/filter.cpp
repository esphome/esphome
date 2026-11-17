#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR_FILTER

#include "filter.h"

#include "binary_sensor.h"
#include "esphome/core/application.h"

namespace esphome::binary_sensor {

ESPHOME_LOG_TAG(TAG, "sensor.filter");

void Filter::output(bool value) {
  if (this->next_ == nullptr) {
    this->parent_->send_state_internal(value);
  } else {
    this->next_->input(value);
  }
}

void Filter::reset() {
  this->dedup_.next_unknown();
  App.scheduler.cancel_timeout(this);
}

void Filter::input(bool value) {
  if (!this->dedup_.next(value))
    return;
  auto b = this->new_value(value);
  if (b.has_value()) {
    this->output(*b);
  }
}

optional<bool> delay_output(Filter *filter, uint32_t delay, bool value) {
  App.scheduler.set_timeout(filter, delay, [filter, value]() { filter->output(value); });
  return {};
}

void TimeoutFilter::input(bool value) {
  App.scheduler.set_timeout(this, this->timeout_delay_.value(), [this]() { this->parent_->invalidate_state(); });
  // Every input re-arms the timeout, including repeats, so this filter must not de-dup
  this->output(value);
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
// Two independent timers per instance, keyed off two stable addresses inside
// the filter: `this` for the timing-step timer, `&active_timing_` for the
// on/off timer. Both are unique per instance and don't collide with anything
// else, so the self-keyed scheduler API is sufficient.
optional<bool> AutorepeatFilterBase::new_value(bool value) {
  if (value) {
    if (this->active_timing_ != 0)
      return {};
    this->next_timing_();
    return true;
  } else {
    App.scheduler.cancel_timeout(this);
    App.scheduler.cancel_timeout(&this->active_timing_);
    this->active_timing_ = 0;
    return false;
  }
}

void AutorepeatFilterBase::reset() {
  this->dedup_.next_unknown();
  // The release cancels both timers; Filter::reset() would leave the on/off timer running
  AutorepeatFilterBase::new_value(false);
}

void AutorepeatFilterBase::next_timing_() {
  if (this->active_timing_ < this->timings_count_) {
    App.scheduler.set_timeout(this, this->timings_[this->active_timing_].delay, [this]() { this->next_timing_(); });
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
  // A callback of that output may have stopped the filter
  if (this->active_timing_ < 2)
    return;
  App.scheduler.set_timeout(&this->active_timing_, val ? timing.time_on : timing.time_off,
                            [this, val]() { this->next_value_(!val); });
}

LambdaFilter::LambdaFilter(std::function<optional<bool>(bool)> f) : f_(std::move(f)) {}

optional<bool> LambdaFilter::new_value(bool value) { return this->f_(value); }

void SettleFilter::reset() {
  Filter::reset();
  this->steady_ = true;
}

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
