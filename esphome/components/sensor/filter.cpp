#include "filter.h"
#include "sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sensor {

static const char *TAG = "sensor.filter";

// Filter
uint32_t Filter::expected_interval(uint32_t input) { return input; }
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
uint32_t Filter::calculate_remaining_interval(uint32_t input) {
  uint32_t this_interval = this->expected_interval(input);
  ESP_LOGVV(TAG, "Filter(%p)::calculate_remaining_interval(%u) -> %u", this, input, this_interval);
  if (this->next_ == nullptr) {
    return this_interval;
  } else {
    return this->next_->calculate_remaining_interval(this_interval);
  }
}

// MedianFilter
MedianFilter::MedianFilter(size_t window_size, size_t send_every, size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void MedianFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void MedianFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> MedianFilter::new_value(float value) {
  if (!isnan(value)) {
    while (this->queue_.size() >= this->window_size_) {
      this->queue_.pop_front();
    }
    this->queue_.push_back(value);
    ESP_LOGVV(TAG, "MedianFilter(%p)::new_value(%f)", this, value);
  }

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float median = 0.0f;
    if (!this->queue_.empty()) {
      std::deque<float> median_queue = this->queue_;
      sort(median_queue.begin(), median_queue.end());

      size_t queue_size = median_queue.size();
      if (queue_size % 2) {
        median = median_queue[queue_size / 2];
      } else {
        median = (median_queue[queue_size / 2] + median_queue[(queue_size / 2) - 1]) / 2.0f;
      }
    }

    ESP_LOGVV(TAG, "MedianFilter(%p)::new_value(%f) SENDING", this, median);
    return median;
  }
  return {};
}

uint32_t MedianFilter::expected_interval(uint32_t input) { return input * this->send_every_; }

// SlidingWindowMovingAverageFilter
SlidingWindowMovingAverageFilter::SlidingWindowMovingAverageFilter(size_t window_size, size_t send_every,
                                                                   size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void SlidingWindowMovingAverageFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void SlidingWindowMovingAverageFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> SlidingWindowMovingAverageFilter::new_value(float value) {
  if (!isnan(value)) {
    if (this->queue_.size() == this->window_size_) {
      this->sum_ -= this->queue_.front();
      this->queue_.pop();
    }
    this->queue_.push(value);
    this->sum_ += value;
  }
  float average;
  if (this->queue_.empty())
    average = 0.0f;
  else
    average = this->sum_ / this->queue_.size();
  ESP_LOGVV(TAG, "SlidingWindowMovingAverageFilter(%p)::new_value(%f) -> %f", this, value, average);

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;
    ESP_LOGVV(TAG, "SlidingWindowMovingAverageFilter(%p)::new_value(%f) SENDING", this, value);
    return average;
  }
  return {};
}

uint32_t SlidingWindowMovingAverageFilter::expected_interval(uint32_t input) { return input * this->send_every_; }

// ExponentialMovingAverageFilter
ExponentialMovingAverageFilter::ExponentialMovingAverageFilter(float alpha, size_t send_every)
    : send_every_(send_every), send_at_(send_every - 1), alpha_(alpha) {}
optional<float> ExponentialMovingAverageFilter::new_value(float value) {
  if (!isnan(value)) {
    if (this->first_value_)
      this->accumulator_ = value;
    else
      this->accumulator_ = (this->alpha_ * value) + (1.0f - this->alpha_) * this->accumulator_;
    this->first_value_ = false;
  }

  float average = this->accumulator_;
  ESP_LOGVV(TAG, "ExponentialMovingAverageFilter(%p)::new_value(%f) -> %f", this, value, average);

  if (++this->send_at_ >= this->send_every_) {
    ESP_LOGVV(TAG, "ExponentialMovingAverageFilter(%p)::new_value(%f) SENDING", this, value);
    this->send_at_ = 0;
    return average;
  }
  return {};
}
void ExponentialMovingAverageFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void ExponentialMovingAverageFilter::set_alpha(float alpha) { this->alpha_ = alpha; }
uint32_t ExponentialMovingAverageFilter::expected_interval(uint32_t input) { return input * this->send_every_; }

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
OffsetFilter::OffsetFilter(float offset) : offset_(offset) {}

optional<float> OffsetFilter::new_value(float value) { return value + this->offset_; }

// MultiplyFilter
MultiplyFilter::MultiplyFilter(float multiplier) : multiplier_(multiplier) {}

optional<float> MultiplyFilter::new_value(float value) { return value * this->multiplier_; }

// FilterOutValueFilter
FilterOutValueFilter::FilterOutValueFilter(float value_to_filter_out) : value_to_filter_out_(value_to_filter_out) {}

optional<float> FilterOutValueFilter::new_value(float value) {
  if (isnan(this->value_to_filter_out_)) {
    if (isnan(value))
      return {};
    else
      return value;
  } else {
    int8_t accuracy = this->parent_->get_accuracy_decimals();
    float accuracy_mult = pow10f(accuracy);
    float rounded_filter_out = roundf(accuracy_mult * this->value_to_filter_out_);
    float rounded_value = roundf(accuracy_mult * value);
    if (rounded_filter_out == rounded_value)
      return {};
    else
      return value;
  }
}

// ThrottleFilter
ThrottleFilter::ThrottleFilter(uint32_t min_time_between_inputs)
    : Filter(), min_time_between_inputs_(min_time_between_inputs) {}
optional<float> ThrottleFilter::new_value(float value) {
  const uint32_t now = millis();
  if (this->last_input_ == 0 || now - this->last_input_ >= min_time_between_inputs_) {
    this->last_input_ = now;
    return value;
  }
  return {};
}

// DeltaFilter
DeltaFilter::DeltaFilter(float min_delta) : min_delta_(min_delta), last_value_(NAN) {}
optional<float> DeltaFilter::new_value(float value) {
  if (isnan(value))
    return {};
  if (isnan(this->last_value_)) {
    return this->last_value_ = value;
  }
  if (fabsf(value - this->last_value_) >= this->min_delta_) {
    return this->last_value_ = value;
  }
  return {};
}

// OrFilter
OrFilter::OrFilter(std::vector<Filter *> filters) : filters_(std::move(filters)), phi_(this) {}
OrFilter::PhiNode::PhiNode(OrFilter *or_parent) : or_parent_(or_parent) {}

optional<float> OrFilter::PhiNode::new_value(float value) {
  this->or_parent_->output(value);

  return {};
}
optional<float> OrFilter::new_value(float value) {
  for (Filter *filter : this->filters_)
    filter->input(value);

  return {};
}
void OrFilter::initialize(Sensor *parent, Filter *next) {
  Filter::initialize(parent, next);
  for (Filter *filter : this->filters_) {
    filter->initialize(parent, &this->phi_);
  }
  this->phi_.initialize(parent, nullptr);
}

uint32_t OrFilter::expected_interval(uint32_t input) {
  uint32_t min_interval = UINT32_MAX;
  for (Filter *filter : this->filters_) {
    min_interval = std::min(min_interval, filter->calculate_remaining_interval(input));
  }

  return min_interval;
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

  return {};
}
uint32_t HeartbeatFilter::expected_interval(uint32_t input) { return this->time_period_; }
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

optional<float> CalibrateLinearFilter::new_value(float value) { return value * this->slope_ + this->bias_; }
CalibrateLinearFilter::CalibrateLinearFilter(float slope, float bias) : slope_(slope), bias_(bias) {}

optional<float> CalibratePolynomialFilter::new_value(float value) {
  float res = 0.0f;
  float x = 1.0f;
  for (float coefficient : this->coefficients_) {
    res += x * coefficient;
    x *= value;
  }
  return res;
}

}  // namespace sensor
}  // namespace esphome
