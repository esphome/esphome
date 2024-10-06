#include "filter.h"
#include <cmath>
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

// MedianFilter
MedianFilter::MedianFilter(size_t window_size, size_t send_every, size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size) {}
void MedianFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void MedianFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
optional<float> MedianFilter::new_value(float value) {
  while (this->queue_.size() >= this->window_size_) {
    this->queue_.pop_front();
  }
  this->queue_.push_back(value);
  ESP_LOGVV(TAG, "MedianFilter(%p)::new_value(%f)", this, value);

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float median = NAN;
    if (!this->queue_.empty()) {
      // Copy queue without NaN values
      std::vector<float> median_queue;
      for (auto v : this->queue_) {
        if (!std::isnan(v)) {
          median_queue.push_back(v);
        }
      }

      sort(median_queue.begin(), median_queue.end());

      size_t queue_size = median_queue.size();
      if (queue_size) {
        if (queue_size % 2) {
          median = median_queue[queue_size / 2];
        } else {
          median = (median_queue[queue_size / 2] + median_queue[(queue_size / 2) - 1]) / 2.0f;
        }
      }
    }

    ESP_LOGVV(TAG, "MedianFilter(%p)::new_value(%f) SENDING %f", this, value, median);
    return median;
  }
  return {};
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
    : send_every_(send_every), send_at_(send_every - send_first_at), window_size_(window_size), quantile_(quantile) {}
void QuantileFilter::set_send_every(size_t send_every) { this->send_every_ = send_every; }
void QuantileFilter::set_window_size(size_t window_size) { this->window_size_ = window_size; }
void QuantileFilter::set_quantile(float quantile) { this->quantile_ = quantile; }
optional<float> QuantileFilter::new_value(float value) {
  while (this->queue_.size() >= this->window_size_) {
    this->queue_.pop_front();
  }
  this->queue_.push_back(value);
  ESP_LOGVV(TAG, "QuantileFilter(%p)::new_value(%f), quantile:%f", this, value, this->quantile_);

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float result = NAN;
    if (!this->queue_.empty()) {
      // Copy queue without NaN values
      std::vector<float> quantile_queue;
      for (auto v : this->queue_) {
        if (!std::isnan(v)) {
          quantile_queue.push_back(v);
        }
      }

      sort(quantile_queue.begin(), quantile_queue.end());

      size_t queue_size = quantile_queue.size();
      if (queue_size) {
        size_t position = ceilf(queue_size * this->quantile_) - 1;
        ESP_LOGVV(TAG, "QuantileFilter(%p)::position: %d/%d", this, position + 1, queue_size);
        result = quantile_queue[position];
      }
    }

    ESP_LOGVV(TAG, "QuantileFilter(%p)::new_value(%f) SENDING %f", this, value, result);
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
  while (this->queue_.size() >= this->window_size_) {
    this->queue_.pop_front();
  }
  this->queue_.push_back(value);
  ESP_LOGVV(TAG, "MinFilter(%p)::new_value(%f)", this, value);

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float min = NAN;
    for (auto v : this->queue_) {
      if (!std::isnan(v)) {
        min = std::isnan(min) ? v : std::min(min, v);
      }
    }

    ESP_LOGVV(TAG, "MinFilter(%p)::new_value(%f) SENDING %f", this, value, min);
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
  while (this->queue_.size() >= this->window_size_) {
    this->queue_.pop_front();
  }
  this->queue_.push_back(value);
  ESP_LOGVV(TAG, "MaxFilter(%p)::new_value(%f)", this, value);

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float max = NAN;
    for (auto v : this->queue_) {
      if (!std::isnan(v)) {
        max = std::isnan(max) ? v : std::max(max, v);
      }
    }

    ESP_LOGVV(TAG, "MaxFilter(%p)::new_value(%f) SENDING %f", this, value, max);
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
  while (this->queue_.size() >= this->window_size_) {
    this->queue_.pop_front();
  }
  this->queue_.push_back(value);
  ESP_LOGVV(TAG, "SlidingWindowMovingAverageFilter(%p)::new_value(%f)", this, value);

  if (++this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float sum = 0;
    size_t valid_count = 0;
    for (auto v : this->queue_) {
      if (!std::isnan(v)) {
        sum += v;
        valid_count++;
      }
    }

    float average = NAN;
    if (valid_count) {
      average = sum / valid_count;
    }

    ESP_LOGVV(TAG, "SlidingWindowMovingAverageFilter(%p)::new_value(%f) SENDING %f", this, value, average);
    return average;
  }
  return {};
}

// ExponentialMovingAverageFilter
ExponentialMovingAverageFilter::ExponentialMovingAverageFilter(float alpha, size_t send_every, size_t send_first_at)
    : send_every_(send_every), send_at_(send_every - send_first_at), alpha_(alpha) {}
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
DeltaFilter::DeltaFilter(float delta, bool percentage_mode)
    : delta_(delta), current_delta_(delta), percentage_mode_(percentage_mode), last_value_(NAN) {}
optional<float> DeltaFilter::new_value(float value) {
  if (std::isnan(value)) {
    if (std::isnan(this->last_value_)) {
      return {};
    } else {
      if (this->percentage_mode_) {
        this->current_delta_ = fabsf(value * this->delta_);
      }
      return this->last_value_ = value;
    }
  }
  if (std::isnan(this->last_value_) || fabsf(value - this->last_value_) >= this->current_delta_) {
    if (this->percentage_mode_) {
      this->current_delta_ = fabsf(value * this->delta_);
    }
    return this->last_value_ = value;
  }
  return {};
}

// OrFilter
OrFilter::OrFilter(std::vector<Filter *> filters) : filters_(std::move(filters)), phi_(this) {}
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

// TimeoutFilter
optional<float> TimeoutFilter::new_value(float value) {
  this->set_timeout("timeout", this->time_period_, [this]() { this->output(this->value_); });
  return value;
}

TimeoutFilter::TimeoutFilter(uint32_t time_period, float new_value) : time_period_(time_period), value_(new_value) {}
float TimeoutFilter::get_setup_priority() const { return setup_priority::HARDWARE; }

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

optional<float> CalibrateLinearFilter::new_value(float value) {
  for (std::array<float, 3> f : this->linear_functions_) {
    if (!std::isfinite(f[2]) || value < f[2])
      return (value * f[0]) + f[1];
  }
  return NAN;
}

optional<float> CalibratePolynomialFilter::new_value(float value) {
  float res = 0.0f;
  float x = 1.0f;
  for (float coefficient : this->coefficients_) {
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

}  // namespace sensor
}  // namespace esphome
