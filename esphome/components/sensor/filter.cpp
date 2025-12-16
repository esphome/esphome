#include "filter.h"
#include <cmath>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "sensor.h"

namespace esphome {
namespace sensor {

static const char *const TAG = "sensor.filter";

// Filter
void Filter::input(float value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%f)", this, value);
  optional<float> out = this->new_value(value);
  if (out.has_value())
    this->output(*out);
}
void Filter::output(float value) {
  if (this->next_ == nullptr) {
    ESP_LOGVV(TAG, "Filter(%p)::output(%f) -> SENSOR", this, value);
    this->parent_->internal_send_state_to_frontend(value);
  } else {
    ESP_LOGVV(TAG, "Filter(%p)::output(%f) -> %p", this, value, this->next_);
    this->next_->input(value);
  }
}
void Filter::initialize(Sensor *parent, Filter *next) {
  ESP_LOGVV(TAG, "Filter(%p)::initialize(parent=%p next=%p)", this, parent, next);
  this->parent_ = parent;
  this->next_ = next;
}

// SlidingWindowFilter
SlidingWindowFilter::SlidingWindowFilter(size_t window_size, size_t send_every, size_t send_first_at)
    : window_size_(window_size), send_every_(send_every), send_at_(send_every - send_first_at) {
  // Allocate ring buffer once at initialization
  this->window_.init(window_size);
}

optional<float> SlidingWindowFilter::new_value(float value) {
  // Add value to ring buffer
  if (this->window_count_ < this->window_size_) {
    // Buffer not yet full - just append
    this->window_.push_back(value);
    this->window_count_++;
  } else {
    // Buffer full - overwrite oldest value (ring buffer)
    this->window_[this->window_head_] = value;
    this->window_head_++;
    if (this->window_head_ >= this->window_size_) {
      this->window_head_ = 0;
    }
  }

  // Check if we should send a result
  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;
    float result = this->compute_result();
    ESP_LOGVV(TAG, "SlidingWindowFilter(%p)::new_value(%f) SENDING %f", this, value, result);
    return result;
  }
  return {};
}

// SortedWindowFilter
FixedVector<float> SortedWindowFilter::get_window_values_() {
  // Copy window without NaN values using FixedVector (no heap allocation)
  // Returns unsorted values - caller will use std::nth_element for partial sorting as needed
  FixedVector<float> values;
  values.init(this->window_count_);
  for (size_t i = 0; i < this->window_count_; i++) {
    float v = this->window_[i];
    if (!std::isnan(v)) {
      values.push_back(v);
    }
  }
  return values;
}

// MedianFilter
float MedianFilter::compute_result() {
  FixedVector<float> values = this->get_window_values_();
  if (values.empty())
    return NAN;

  size_t size = values.size();
  size_t mid = size / 2;

  if (size % 2) {
    // Odd number of elements - use nth_element to find middle element
    std::nth_element(values.begin(), values.begin() + mid, values.end());
    return values[mid];
  }
  // Even number of elements - need both middle elements
  // Use nth_element to find upper middle element
  std::nth_element(values.begin(), values.begin() + mid, values.end());
  float upper = values[mid];
  // Find the maximum of the lower half (which is now everything before mid)
  float lower = *std::max_element(values.begin(), values.begin() + mid);
  return (lower + upper) / 2.0f;
}

// SkipInitialFilter
SkipInitialFilter::SkipInitialFilter(size_t num_to_ignore) : num_to_ignore_(num_to_ignore) {}
optional<float> SkipInitialFilter::new_value(float value) {
  if (num_to_ignore_ > 0) {
    num_to_ignore_--;
    ESP_LOGV(TAG, "SkipInitialFilter(%p)::new_value(%f) SKIPPING, %zu left", this, value, num_to_ignore_);
    return {};
  }

  ESP_LOGV(TAG, "SkipInitialFilter(%p)::new_value(%f) SENDING", this, value);
  return value;
}

// QuantileFilter
QuantileFilter::QuantileFilter(size_t window_size, size_t send_every, size_t send_first_at, float quantile)
    : SortedWindowFilter(window_size, send_every, send_first_at), quantile_(quantile) {}

float QuantileFilter::compute_result() {
  FixedVector<float> values = this->get_window_values_();
  if (values.empty())
    return NAN;

  size_t position = ceilf(values.size() * this->quantile_) - 1;
  ESP_LOGVV(TAG, "QuantileFilter(%p)::position: %zu/%zu", this, position + 1, values.size());

  // Use nth_element to find the quantile element (O(n) instead of O(n log n))
  std::nth_element(values.begin(), values.begin() + position, values.end());
  return values[position];
}

// MinFilter
float MinFilter::compute_result() { return this->find_extremum_<std::less<float>>(); }

// MaxFilter
float MaxFilter::compute_result() { return this->find_extremum_<std::greater<float>>(); }

// SlidingWindowMovingAverageFilter
float SlidingWindowMovingAverageFilter::compute_result() {
  float sum = 0;
  size_t valid_count = 0;
  for (size_t i = 0; i < this->window_count_; i++) {
    float v = this->window_[i];
    if (!std::isnan(v)) {
      sum += v;
      valid_count++;
    }
  }
  return valid_count ? sum / valid_count : NAN;
}

// ExponentialMovingAverageFilter
ExponentialMovingAverageFilter::ExponentialMovingAverageFilter(float alpha, size_t send_every, size_t send_first_at)
    : alpha_(alpha), send_every_(send_every), send_at_(send_every - send_first_at) {}
optional<float> ExponentialMovingAverageFilter::new_value(float value) {
  if (!std::isnan(value)) {
    if (this->first_value_) {
      this->accumulator_ = value;
      this->first_value_ = false;
    } else {
      this->accumulator_ = (this->alpha_ * value) + (1.0f - this->alpha_) * this->accumulator_;
    }
  }

  const float average = std::isnan(value) ? value : this->accumulator_;
  ESP_LOGVV(TAG, "ExponentialMovingAverageFilter(%p)::new_value(%f) -> %f", this, value, average);

  if (++this->send_at_ >= this->send_every_) {
    ESP_LOGVV(TAG, "ExponentialMovingAverageFilter(%p)::new_value(%f) SENDING %f", this, value, average);
    this->send_at_ = 0;
    return average;
  }
  return {};
}
void ExponentialMovingAverageFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void ExponentialMovingAverageFilter::set_alpha(float alpha) { this->alpha_ = alpha; }

// ThrottleAverageFilter
ThrottleAverageFilter::ThrottleAverageFilter(uint32_t time_period) : time_period_(time_period) {}

optional<float> ThrottleAverageFilter::new_value(float value) {
  ESP_LOGVV(TAG, "ThrottleAverageFilter(%p)::new_value(value=%f)", this, value);
  if (std::isnan(value)) {
    this->have_nan_ = true;
  } else {
    this->sum_ += value;
    this->n_++;
  }
  return {};
}
void ThrottleAverageFilter::setup() {
  this->set_interval("throttle_average", this->time_period_, [this]() {
    ESP_LOGVV(TAG, "ThrottleAverageFilter(%p)::interval(sum=%f, n=%i)", this, this->sum_, this->n_);
    if (this->n_ == 0) {
      if (this->have_nan_)
        this->output(NAN);
    } else {
      this->output(this->sum_ / this->n_);
      this->sum_ = 0.0f;
      this->n_ = 0;
    }
    this->have_nan_ = false;
  });
}
float ThrottleAverageFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

// LambdaFilter
LambdaFilter::LambdaFilter(lambda_filter_t lambda_filter) : lambda_filter_(std::move(lambda_filter)) {}
const lambda_filter_t &LambdaFilter::get_lambda_filter() const { return this->lambda_filter_; }
void LambdaFilter::set_lambda_filter(const lambda_filter_t &lambda_filter) { this->lambda_filter_ = lambda_filter; }

optional<float> LambdaFilter::new_value(float value) {
  auto it = this->lambda_filter_(value);
  ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%f) -> %f", this, value, it.value_or(INFINITY));
  return it;
}

// OffsetFilter
OffsetFilter::OffsetFilter(TemplatableValue<float> offset) : offset_(std::move(offset)) {}

optional<float> OffsetFilter::new_value(float value) { return value + this->offset_.value(); }

// MultiplyFilter
MultiplyFilter::MultiplyFilter(TemplatableValue<float> multiplier) : multiplier_(std::move(multiplier)) {}

optional<float> MultiplyFilter::new_value(float value) { return value * this->multiplier_.value(); }

// ValueListFilter (base class)
ValueListFilter::ValueListFilter(std::initializer_list<TemplatableValue<float>> values) : values_(values) {}

bool ValueListFilter::value_matches_any_(float sensor_value) {
  int8_t accuracy = this->parent_->get_accuracy_decimals();
  float accuracy_mult = powf(10.0f, accuracy);
  float rounded_sensor = roundf(accuracy_mult * sensor_value);

  for (auto &filter_value : this->values_) {
    float fv = filter_value.value();

    // Handle NaN comparison
    if (std::isnan(fv)) {
      if (std::isnan(sensor_value))
        return true;
      continue;
    }

    // Compare rounded values
    if (roundf(accuracy_mult * fv) == rounded_sensor)
      return true;
  }

  return false;
}

// FilterOutValueFilter
FilterOutValueFilter::FilterOutValueFilter(std::initializer_list<TemplatableValue<float>> values_to_filter_out)
    : ValueListFilter(values_to_filter_out) {}

optional<float> FilterOutValueFilter::new_value(float value) {
  if (this->value_matches_any_(value))
    return {};   // Filter out
  return value;  // Pass through
}

// ThrottleFilter
ThrottleFilter::ThrottleFilter(uint32_t min_time_between_inputs) : min_time_between_inputs_(min_time_between_inputs) {}
optional<float> ThrottleFilter::new_value(float value) {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_input_ == 0 || now - this->last_input_ >= min_time_between_inputs_) {
    this->last_input_ = now;
    return value;
  }
  return {};
}

// ThrottleWithPriorityFilter
ThrottleWithPriorityFilter::ThrottleWithPriorityFilter(
    uint32_t min_time_between_inputs, std::initializer_list<TemplatableValue<float>> prioritized_values)
    : ValueListFilter(prioritized_values), min_time_between_inputs_(min_time_between_inputs) {}

optional<float> ThrottleWithPriorityFilter::new_value(float value) {
  const uint32_t now = App.get_loop_component_start_time();
  // Allow value through if: no previous input, time expired, or is prioritized
  if (this->last_input_ == 0 || now - this->last_input_ >= min_time_between_inputs_ ||
      this->value_matches_any_(value)) {
    this->last_input_ = now;
    return value;
  }
  return {};
}

// DeltaFilter
DeltaFilter::DeltaFilter(float delta, bool percentage_mode)
    : delta_(delta), current_delta_(delta), last_value_(NAN), percentage_mode_(percentage_mode) {}
optional<float> DeltaFilter::new_value(float value) {
  if (std::isnan(value)) {
    if (std::isnan(this->last_value_)) {
      return {};
    } else {
      return this->last_value_ = value;
    }
  }
  float diff = fabsf(value - this->last_value_);
  if (std::isnan(this->last_value_) || (diff > 0.0f && diff >= this->current_delta_)) {
    if (this->percentage_mode_) {
      this->current_delta_ = fabsf(value * this->delta_);
    }
    return this->last_value_ = value;
  }
  return {};
}

// OrFilter
OrFilter::OrFilter(std::initializer_list<Filter *> filters) : filters_(filters), phi_(this) {}
OrFilter::PhiNode::PhiNode(OrFilter *or_parent) : or_parent_(or_parent) {}

optional<float> OrFilter::PhiNode::new_value(float value) {
  if (!this->or_parent_->has_value_) {
    this->or_parent_->output(value);
    this->or_parent_->has_value_ = true;
  }

  return {};
}
optional<float> OrFilter::new_value(float value) {
  this->has_value_ = false;
  for (auto *filter : this->filters_)
    filter->input(value);

  return {};
}
void OrFilter::initialize(Sensor *parent, Filter *next) {
  Filter::initialize(parent, next);
  for (auto *filter : this->filters_) {
    filter->initialize(parent, &this->phi_);
  }
  this->phi_.initialize(parent, nullptr);
}

// TimeoutFilterBase - shared loop logic
void TimeoutFilterBase::loop() {
  // Check if timeout period has elapsed
  // Use cached loop start time to avoid repeated millis() calls
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->timeout_start_time_ >= this->time_period_) {
    // Timeout fired - get output value from derived class and output it
    this->output(this->get_output_value());

    // Disable loop until next value arrives
    this->disable_loop();
  }
}

float TimeoutFilterBase::get_setup_priority() const { return setup_priority::HARDWARE; }

// TimeoutFilterLast - "last" mode implementation
optional<float> TimeoutFilterLast::new_value(float value) {
  // Store the value to output when timeout fires
  this->pending_value_ = value;

  // Record when timeout started and enable loop
  this->timeout_start_time_ = millis();
  this->enable_loop();

  return value;
}

// TimeoutFilterConfigured - configured value mode implementation
optional<float> TimeoutFilterConfigured::new_value(float value) {
  // Record when timeout started and enable loop
  // Note: we don't store the incoming value since we have a configured value
  this->timeout_start_time_ = millis();
  this->enable_loop();

  return value;
}

// DebounceFilter
optional<float> DebounceFilter::new_value(float value) {
  this->set_timeout("debounce", this->time_period_, [this, value]() { this->output(value); });

  return {};
}

DebounceFilter::DebounceFilter(uint32_t time_period) : time_period_(time_period) {}
float DebounceFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

// HeartbeatFilter
HeartbeatFilter::HeartbeatFilter(uint32_t time_period) : time_period_(time_period), last_input_(NAN) {}

optional<float> HeartbeatFilter::new_value(float value) {
  ESP_LOGVV(TAG, "HeartbeatFilter(%p)::new_value(value=%f)", this, value);
  this->last_input_ = value;
  this->has_value_ = true;

  if (this->optimistic_) {
    return value;
  }
  return {};
}

void HeartbeatFilter::setup() {
  this->set_interval("heartbeat", this->time_period_, [this]() {
    ESP_LOGVV(TAG, "HeartbeatFilter(%p)::interval(has_value=%s, last_input=%f)", this, YESNO(this->has_value_),
              this->last_input_);
    if (!this->has_value_)
      return;

    this->output(this->last_input_);
  });
}

float HeartbeatFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

CalibrateLinearFilter::CalibrateLinearFilter(std::initializer_list<std::array<float, 3>> linear_functions)
    : linear_functions_(linear_functions) {}

optional<float> CalibrateLinearFilter::new_value(float value) {
  for (const auto &f : this->linear_functions_) {
    if (!std::isfinite(f[2]) || value < f[2])
      return (value * f[0]) + f[1];
  }
  return NAN;
}

CalibratePolynomialFilter::CalibratePolynomialFilter(std::initializer_list<float> coefficients)
    : coefficients_(coefficients) {}

optional<float> CalibratePolynomialFilter::new_value(float value) {
  float res = 0.0f;
  float x = 1.0f;
  for (const auto &coefficient : this->coefficients_) {
    res += x * coefficient;
    x *= value;
  }
  return res;
}

ClampFilter::ClampFilter(float min, float max, bool ignore_out_of_range)
    : min_(min), max_(max), ignore_out_of_range_(ignore_out_of_range) {}
optional<float> ClampFilter::new_value(float value) {
  if (std::isfinite(value)) {
    if (std::isfinite(this->min_) && value < this->min_) {
      if (this->ignore_out_of_range_) {
        return {};
      } else {
        return this->min_;
      }
    }

    if (std::isfinite(this->max_) && value > this->max_) {
      if (this->ignore_out_of_range_) {
        return {};
      } else {
        return this->max_;
      }
    }
  }
  return value;
}

RoundFilter::RoundFilter(uint8_t precision) : precision_(precision) {}
optional<float> RoundFilter::new_value(float value) {
  if (std::isfinite(value)) {
    float accuracy_mult = powf(10.0f, this->precision_);
    return roundf(accuracy_mult * value) / accuracy_mult;
  }
  return value;
}

RoundMultipleFilter::RoundMultipleFilter(float multiple) : multiple_(multiple) {}
optional<float> RoundMultipleFilter::new_value(float value) {
  if (std::isfinite(value)) {
    return value - remainderf(value, this->multiple_);
  }
  return value;
}

optional<float> ToNTCResistanceFilter::new_value(float value) {
  if (!std::isfinite(value)) {
    return NAN;
  }
  double k = 273.15;
  // https://de.wikipedia.org/wiki/Steinhart-Hart-Gleichung#cite_note-stein2_s4-3
  double t = value + k;
  double y = (this->a_ - 1 / (t)) / (2 * this->c_);
  double x = sqrt(pow(this->b_ / (3 * this->c_), 3) + y * y);
  double resistance = exp(pow(x - y, 1 / 3.0) - pow(x + y, 1 / 3.0));
  return resistance;
}

optional<float> ToNTCTemperatureFilter::new_value(float value) {
  if (!std::isfinite(value)) {
    return NAN;
  }
  double lr = log(double(value));
  double v = this->a_ + this->b_ * lr + this->c_ * lr * lr * lr;
  double temp = float(1.0 / v - 273.15);
  return temp;
}

// StreamingFilter (base class)
StreamingFilter::StreamingFilter(size_t window_size, size_t send_first_at)
    : window_size_(window_size), send_first_at_(send_first_at) {}

optional<float> StreamingFilter::new_value(float value) {
  // Process the value (child class tracks min/max/sum/etc)
  this->process_value(value);

  this->count_++;

  // Check if we should send (handle send_first_at for first value)
  bool should_send = false;
  if (this->first_send_ && this->count_ >= this->send_first_at_) {
    should_send = true;
    this->first_send_ = false;
  } else if (!this->first_send_ && this->count_ >= this->window_size_) {
    should_send = true;
  }

  if (should_send) {
    float result = this->compute_batch_result();
    // Reset for next batch
    this->count_ = 0;
    this->reset_batch();
    ESP_LOGVV(TAG, "StreamingFilter(%p)::new_value(%f) SENDING %f", this, value, result);
    return result;
  }

  return {};
}

// StreamingMinFilter
void StreamingMinFilter::process_value(float value) {
  // Update running minimum (ignore NaN values)
  if (!std::isnan(value)) {
    this->current_min_ = std::isnan(this->current_min_) ? value : std::min(this->current_min_, value);
  }
}

float StreamingMinFilter::compute_batch_result() { return this->current_min_; }

void StreamingMinFilter::reset_batch() { this->current_min_ = NAN; }

// StreamingMaxFilter
void StreamingMaxFilter::process_value(float value) {
  // Update running maximum (ignore NaN values)
  if (!std::isnan(value)) {
    this->current_max_ = std::isnan(this->current_max_) ? value : std::max(this->current_max_, value);
  }
}

float StreamingMaxFilter::compute_batch_result() { return this->current_max_; }

void StreamingMaxFilter::reset_batch() { this->current_max_ = NAN; }

// StreamingMovingAverageFilter
void StreamingMovingAverageFilter::process_value(float value) {
  // Accumulate sum (ignore NaN values)
  if (!std::isnan(value)) {
    this->sum_ += value;
    this->valid_count_++;
  }
}

float StreamingMovingAverageFilter::compute_batch_result() {
  return this->valid_count_ > 0 ? this->sum_ / this->valid_count_ : NAN;
}

void StreamingMovingAverageFilter::reset_batch() {
  this->sum_ = 0.0f;
  this->valid_count_ = 0;
}

}  // namespace sensor
}  // namespace esphome
