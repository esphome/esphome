#include "filter.h"

#include "binary_sensor.h"

namespace esphome::binary_sensor {

static const char *const TAG = "sensor.filter";

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
  this->set_timeout("timeout", this->timeout_delay_.value(), [this]() { this->parent_->invalidate_state(); });
  // we do not de-dup here otherwise changes from invalid to valid state will not be output
  this->output(value);
}

optional<bool> DelayedOnOffFilter::new_value(bool value) {
  if (value) {
    this->set_timeout("ON_OFF", this->on_delay_.value(), [this]() { this->output(true); });
  } else {
    this->set_timeout("ON_OFF", this->off_delay_.value(), [this]() { this->output(false); });
  }
  return {};
}

float DelayedOnOffFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

optional<bool> DelayedOnFilter::new_value(bool value) {
  if (value) {
    this->set_timeout("ON", this->delay_.value(), [this]() { this->output(true); });
    return {};
  } else {
    this->cancel_timeout("ON");
    return false;
  }
}

float DelayedOnFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

optional<bool> DelayedOffFilter::new_value(bool value) {
  if (!value) {
    this->set_timeout("OFF", this->delay_.value(), [this]() { this->output(false); });
    return {};
  } else {
    this->cancel_timeout("OFF");
    return true;
  }
}

float DelayedOffFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

optional<bool> InvertFilter::new_value(bool value) { return !value; }

AutorepeatFilter::AutorepeatFilter(std::initializer_list<AutorepeatFilterTiming> timings) : timings_(timings) {}

optional<bool> AutorepeatFilter::new_value(bool value) {
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
  this->output(val);  // This is at least the second one so not initial
  this->set_timeout("ON_OFF", val ? timing.time_on : timing.time_off, [this, val]() { this->next_value_(!val); });
}

float AutorepeatFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

LambdaFilter::LambdaFilter(std::function<optional<bool>(bool)> f) : f_(std::move(f)) {}

optional<bool> LambdaFilter::new_value(bool value) { return this->f_(value); }

optional<bool> SettleFilter::new_value(bool value) {
  if (!this->steady_) {
    this->set_timeout("SETTLE", this->delay_.value(), [this, value]() {
      this->steady_ = true;
      this->output(value);
    });
    return {};
  } else {
    this->steady_ = false;
    this->output(value);
    this->set_timeout("SETTLE", this->delay_.value(), [this]() { this->steady_ = true; });
    return value;
  }
}

float SettleFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esphome::binary_sensor
