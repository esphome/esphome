#include "filter.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "sensor.h"
#include <cmath>

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

// MedianFilter
MedianFilter::MedianFilter(size_t window_size, size_t send_every, size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void MedianFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void MedianFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> MedianFilter::new_value(float value) {
  if (!std::isnan(value)) {
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

// QuantileFilter
QuantileFilter::QuantileFilter(size_t window_size, size_t send_every, size_t send_first_at, float quantile)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size), quantile_(quantile) {}
void QuantileFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void QuantileFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
void QuantileFilter::set_quantile(float quantile) { this->quantile_ = quantile; }
optional<float> QuantileFilter::new_value(float value) {
  if (!std::isnan(value)) {
    while (this->queue_.size() >= this->window_size_) {
      this->queue_.pop_front();
    }
    this->queue_.push_back(value);
    ESP_LOGVV(TAG, "QuantileFilter(%p)::new_value(%f), quantile:%f", this, value, this->quantile_);
  }

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float result = 0.0f;
    if (!this->queue_.empty()) {
      std::deque<float> quantile_queue = this->queue_;
      sort(quantile_queue.begin(), quantile_queue.end());

      size_t queue_size = quantile_queue.size();
      size_t position = ceilf(queue_size * this->quantile_) - 1;
      ESP_LOGVV(TAG, "QuantileFilter(%p)::position: %d/%d", this, position, queue_size);
      result = quantile_queue[position];
    }

    ESP_LOGVV(TAG, "QuantileFilter(%p)::new_value(%f) SENDING", this, result);
    return result;
  }
  return {};
}

// MinFilter
MinFilter::MinFilter(size_t window_size, size_t send_every, size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void MinFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void MinFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> MinFilter::new_value(float value) {
  if (!std::isnan(value)) {
    while (this->queue_.size() >= this->window_size_) {
      this->queue_.pop_front();
    }
    this->queue_.push_back(value);
    ESP_LOGVV(TAG, "MinFilter(%p)::new_value(%f)", this, value);
  }

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float min = 0.0f;
    if (!this->queue_.empty()) {
      std::deque<float>::iterator it = std::min_element(queue_.begin(), queue_.end());
      min = *it;
    }

    ESP_LOGVV(TAG, "MinFilter(%p)::new_value(%f) SENDING", this, min);
    return min;
  }
  return {};
}

// MaxFilter
MaxFilter::MaxFilter(size_t window_size, size_t send_every, size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void MaxFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void MaxFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> MaxFilter::new_value(float value) {
  if (!std::isnan(value)) {
    while (this->queue_.size() >= this->window_size_) {
      this->queue_.pop_front();
    }
    this->queue_.push_back(value);
    ESP_LOGVV(TAG, "MaxFilter(%p)::new_value(%f)", this, value);
  }

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float max = 0.0f;
    if (!this->queue_.empty()) {
      std::deque<float>::iterator it = std::max_element(queue_.begin(), queue_.end());
      max = *it;
    }

    ESP_LOGVV(TAG, "MaxFilter(%p)::new_value(%f) SENDING", this, max);
    return max;
  }
  return {};
}

// SlidingWindowMovingAverageFilter
SlidingWindowMovingAverageFilter::SlidingWindowMovingAverageFilter(size_t window_size, size_t send_every,
                                                                   size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void SlidingWindowMovingAverageFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void SlidingWindowMovingAverageFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> SlidingWindowMovingAverageFilter::new_value(float value) {
  if (!std::isnan(value)) {
    if (this->queue_.size() == this->window_size_) {
      this->sum_ -= this->queue_[0];
      this->queue_.pop_front();
    }
    this->queue_.push_back(value);
    this->sum_ += value;
  }
  float average;
  if (this->queue_.empty()) {
    average = 0.0f;
  } else {
    average = this->sum_ / this->queue_.size();
  }
  ESP_LOGVV(TAG, "SlidingWindowMovingAverageFilter(%p)::new_value(%f) -> %f", this, value, average);

  if (++this->send_at_ % this->send_every_ == 0) {
    if (this->send_at_ >= 10000) {
      // Recalculate to prevent floating point error accumulating
      this->sum_ = 0;
      for (auto v : this->queue_)
        this->sum_ += v;
      average = this->sum_ / this->queue_.size();
      this->send_at_ = 0;
    }

    ESP_LOGVV(TAG, "SlidingWindowMovingAverageFilter(%p)::new_value(%f) SENDING", this, value);
    return average;
  }
  return {};
}

// ExponentialMovingAverageFilter
ExponentialMovingAverageFilter::ExponentialMovingAverageFilter(float alpha, size_t send_every)
    : send_every_(send_every), send_at_(send_every - 1), alpha_(alpha) {}
optional<float> ExponentialMovingAverageFilter::new_value(float value) {
  if (!std::isnan(value)) {
    if (this->first_value_) {
      this->accumulator_ = value;
    } else {
      this->accumulator_ = (this->alpha_ * value) + (1.0f - this->alpha_) * this->accumulator_;
    }
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

// ThrottleAverageFilter
ThrottleAverageFilter::ThrottleAverageFilter(uint32_t time_period) : time_period_(time_period) {}

optional<float> ThrottleAverageFilter::new_value(float value) {
  ESP_LOGVV(TAG, "ThrottleAverageFilter(%p)::new_value(value=%f)", this, value);
  if (!std::isnan(value)) {
    this->sum_ += value;
    this->n_++;
  }
  return {};
}
void ThrottleAverageFilter::setup() {
  this->set_interval("throttle_average", this->time_period_, [this]() {
    ESP_LOGVV(TAG, "ThrottleAverageFilter(%p)::interval(sum=%f, n=%i)", this, this->sum_, this->n_);
    if (this->n_ == 0) {
      this->output(NAN);
    } else {
      this->output(this->sum_ / this->n_);
      this->sum_ = 0.0f;
      this->n_ = 0;
    }
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
OffsetFilter::OffsetFilter(float offset) : offset_(offset) {}

optional<float> OffsetFilter::new_value(float value) { return value + this->offset_; }

// MultiplyFilter
MultiplyFilter::MultiplyFilter(float multiplier) : multiplier_(multiplier) {}

optional<float> MultiplyFilter::new_value(float value) { return value * this->multiplier_; }

// FilterOutValueFilter
FilterOutValueFilter::FilterOutValueFilter(float value_to_filter_out) : value_to_filter_out_(value_to_filter_out) {}

optional<float> FilterOutValueFilter::new_value(float value) {
  if (std::isnan(this->value_to_filter_out_)) {
    if (std::isnan(value)) {
      return {};
    } else {
      return value;
    }
  } else {
    int8_t accuracy = this->parent_->get_accuracy_decimals();
    float accuracy_mult = powf(10.0f, accuracy);
    float rounded_filter_out = roundf(accuracy_mult * this->value_to_filter_out_);
    float rounded_value = roundf(accuracy_mult * value);
    if (rounded_filter_out == rounded_value) {
      return {};
    } else {
      return value;
    }
  }
}

// ThrottleFilter
ThrottleFilter::ThrottleFilter(uint32_t min_time_between_inputs) : min_time_between_inputs_(min_time_between_inputs) {}
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
  if (std::isnan(value))
    return {};
  if (std::isnan(this->last_value_)) {
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
