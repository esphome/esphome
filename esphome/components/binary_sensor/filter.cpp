#include "filter.h"
#include "binary_sensor.h"

namespace esphome {

namespace binary_sensor {

static const char *TAG = "sensor.filter";

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

LambdaFilter::LambdaFilter(const std::function<optional<bool>(bool)> &f) : f_(f) {}

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
