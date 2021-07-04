#include "filter.h"

#include "binary_sensor.h"
#include <utility>

namespace esphome {

namespace binary_sensor {

static const char *const TAG = "sensor.filter";

void Filter::output(bool value, bool is_initial) {
  if (!this->dedup_.next(value))
    return;

  if (this->next_ == nullptr) {
    this->parent_->send_state_internal(value, is_initial);
  } else {
    this->next_->input(value, is_initial);
  }
}
void Filter::input(bool value, bool is_initial) {
  auto b = this->new_value(value, is_initial);
  if (b.has_value()) {
    this->output(*b, is_initial);
  }
}

DelayedOnOffFilter::DelayedOnOffFilter(uint32_t delay) : delay_(delay) {}
optional<bool> DelayedOnOffFilter::new_value(bool value, bool is_initial) {
  if (value) {
    this->set_timeout("ON_OFF", this->delay_, [this, is_initial]() { this->output(true, is_initial); });
  } else {
    this->set_timeout("ON_OFF", this->delay_, [this, is_initial]() { this->output(false, is_initial); });
  }
  return {};
}

float DelayedOnOffFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

DelayedOnFilter::DelayedOnFilter(uint32_t delay) : delay_(delay) {}
optional<bool> DelayedOnFilter::new_value(bool value, bool is_initial) {
  if (value) {
    this->set_timeout("ON", this->delay_, [this, is_initial]() { this->output(true, is_initial); });
    return {};
  } else {
    this->cancel_timeout("ON");
    return false;
  }
}

float DelayedOnFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

DelayedOffFilter::DelayedOffFilter(uint32_t delay) : delay_(delay) {}
optional<bool> DelayedOffFilter::new_value(bool value, bool is_initial) {
  if (!value) {
    this->set_timeout("OFF", this->delay_, [this, is_initial]() { this->output(false, is_initial); });
    return {};
  } else {
    this->cancel_timeout("OFF");
    return true;
  }
}

float DelayedOffFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

optional<bool> InvertFilter::new_value(bool value, bool is_initial) { return !value; }

AutorepeatFilter::AutorepeatFilter(std::vector<AutorepeatFilterTiming> timings) : timings_(std::move(timings)) {}

optional<bool> AutorepeatFilter::new_value(bool value, bool is_initial) {
  if (value) {
    // Ignore if already running
    if (this->active_timing_ != 0)
      return {};

    this->next_timing_();
    return true;
  } else {
    this->cancel_timeout("TIMING");
    this->cancel_timeout("ON_OFF");
    this->active_timing_ = 0;
    return false;
  }
}

void AutorepeatFilter::next_timing_() {
  // Entering this method
  // 1st time: starts waiting the first delay
  // 2nd time: starts waiting the second delay and starts toggling with the first time_off / _on
  // last time: no delay to start but have to bump the index to reflect the last
  if (this->active_timing_ < this->timings_.size())
    this->set_timeout("TIMING", this->timings_[this->active_timing_].delay, [this]() { this->next_timing_(); });

  if (this->active_timing_ <= this->timings_.size()) {
    this->active_timing_++;
  }

  if (this->active_timing_ == 2)
    this->next_value_(false);

  // Leaving this method: if the toggling is started, it has to use [active_timing_ - 2] for the intervals
}

void AutorepeatFilter::next_value_(bool val) {
  const AutorepeatFilterTiming &timing = this->timings_[this->active_timing_ - 2];
  this->output(val, false);  // This is at least the second one so not initial
  this->set_timeout("ON_OFF", val ? timing.time_on : timing.time_off, [this, val]() { this->next_value_(!val); });
}

float AutorepeatFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

LambdaFilter::LambdaFilter(std::function<optional<bool>(bool)> f) : f_(std::move(f)) {}

optional<bool> LambdaFilter::new_value(bool value, bool is_initial) { return this->f_(value); }

optional<bool> UniqueFilter::new_value(bool value, bool is_initial) {
  if (this->last_value_.has_value() && *this->last_value_ == value) {
    return {};
  } else {
    this->last_value_ = value;
    return value;
  }
}

}  // namespace binary_sensor

}  // namespace esphome
