#include "esphome/core/defines.h"
#ifdef USE_TEXT_SENSOR_FILTER

#include "filter.h"
#include "text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::text_sensor {

ESPHOME_LOG_TAG(TAG, "text_sensor.filter");

// Filter
void Filter::input(std::string value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%s)", this, value.c_str());
  if (this->new_value(value))
    this->output(value);
}
void Filter::output(std::string &value) {
  if (this->next_ == nullptr) {
    ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> SENSOR", this, value.c_str());
    this->parent_->internal_send_state_to_frontend(value);
  } else {
    ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> %p", this, value.c_str(), this->next_);
    this->next_->input(std::move(value));
  }
}
void Filter::initialize(TextSensor *parent, Filter *next) {
  ESP_LOGVV(TAG, "Filter(%p)::initialize(parent=%p next=%p)", this, parent, next);
  this->parent_ = parent;
  this->next_ = next;
}

// LambdaFilter
LambdaFilter::LambdaFilter(lambda_filter_t lambda_filter) : lambda_filter_(std::move(lambda_filter)) {}
const lambda_filter_t &LambdaFilter::get_lambda_filter() const { return this->lambda_filter_; }
void LambdaFilter::set_lambda_filter(const lambda_filter_t &lambda_filter) { this->lambda_filter_ = lambda_filter; }

bool LambdaFilter::new_value(std::string &value) {
  auto result = this->lambda_filter_(value);
  if (result.has_value()) {
    ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%s) -> %s (continue)", this, value.c_str(), result->c_str());
  } else {
    ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%s) -> (stop)", this, value.c_str());
  }
  return apply_lambda_filter_result(value, std::move(result));
}

bool apply_lambda_filter_result(std::string &value, optional<std::string> &&result) {
  if (!result.has_value())
    return false;
  value = std::move(*result);
  return true;
}

// ToUpperFilter
bool ToUpperFilter::new_value(std::string &value) {
  for (char &c : value)
    c = ::toupper(c);
  return true;
}

// ToLowerFilter
bool ToLowerFilter::new_value(std::string &value) {
  for (char &c : value)
    c = ::tolower(c);
  return true;
}

// Append
bool AppendFilter::new_value(std::string &value) {
  value.append(this->suffix_);
  return true;
}

// Prepend
bool PrependFilter::new_value(std::string &value) {
  value.insert(0, this->prefix_);
  return true;
}

// Substitute
bool SubstituteFilter::new_value(std::string &value) {
  for (const Substitution *s = this->substitutions_; s->from != nullptr; s++) {
    const size_t from_len = strlen(s->from);
    const size_t to_len = strlen(s->to);
    std::size_t pos = 0;
    while ((pos = value.find(s->from, pos, from_len)) != std::string::npos) {
      value.replace(pos, from_len, s->to, to_len);
      pos += to_len;
    }
  }
  return true;
}

// Map
bool MapFilter::new_value(std::string &value) {
  for (const Substitution *m = this->mappings_; m->from != nullptr; m++) {
    if (value == m->from) {
      value.assign(m->to);
      return true;
    }
  }
  return true;
}

// Throttle
// True when a value may pass: the period is over, or the value is in the nullptr-ended `prioritized` list
static bool throttle_passes(uint32_t &last_input, uint32_t period, const std::string &value,
                            const char *const *prioritized) {
  const uint32_t now = App.get_loop_component_start_time();
  bool pass = last_input == 0 || now - last_input >= period;
  for (const char *const *p = prioritized; !pass && p != nullptr && *p != nullptr; p++)
    pass = value == *p;
  if (pass)
    last_input = now;
  return pass;
}

bool ThrottleFilter::new_value(std::string &value) {
  return throttle_passes(this->last_input_, this->min_time_between_inputs_, value, nullptr);
}

bool ThrottleWithPriorityFilter::new_value(std::string &value) {
  return throttle_passes(this->last_input_, this->min_time_between_inputs_, value, this->prioritized_);
}

// Timeout
bool TimeoutFilterBase::new_value(std::string &value) {
  this->timeout_start_time_ = App.get_loop_component_start_time();
  this->enable_loop();
  return true;
}

void TimeoutFilterBase::loop() {
  if (App.get_loop_component_start_time() - this->timeout_start_time_ < this->time_period_)
    return;
  // Before sending, so a callback that publishes to this sensor again re-arms the timeout
  this->disable_loop();
  this->send_timeout_value();
}

void TimeoutFilterBase::send_(const char *text, size_t len) {
  if (this->next_ == nullptr) {
    this->parent_->internal_send_state_to_frontend(text, len);
  } else {
    this->next_->input(std::string(text, len));
  }
}

bool TimeoutFilterLast::new_value(std::string &value) {
  this->last_ = value;  // reuses last_'s buffer
  return TimeoutFilterBase::new_value(value);
}

bool TimeoutThrottleFilter::new_value(std::string &value) {
  TimeoutFilterLast::new_value(value);
  return throttle_passes(this->last_input_, this->time_period_, value, this->prioritized_);
}

void TimeoutThrottleFilter::send_timeout_value() {
  if (throttle_passes(this->last_input_, this->time_period_, this->last_, this->prioritized_))
    this->send_(this->last_.data(), this->last_.size());
}

}  // namespace esphome::text_sensor

#endif  // USE_TEXT_SENSOR_FILTER
