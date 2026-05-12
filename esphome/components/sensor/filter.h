#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SENSOR_FILTER

#include <array>
#include <utility>
#include <vector>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::sensor {

class Sensor;

/** Apply a filter to sensor values such as moving average.
 *
 * This class is purposefully kept quite simple, since more complicated
 * filters should really be done with the filter sensor in Home Assistant.
 */
class Filter {
 public:
  /** This will be called every time the filter receives a new value.
   *
   * It can return an empty optional to indicate that the filter chain
   * should stop, otherwise the value in the filter will be passed down
   * the chain.
   *
   * @param value The new value.
   * @return An optional float, the new value that should be pushed out.
   */
  virtual optional<float> new_value(float value) = 0;

  /// Initialize this filter, please note this can be called more than once.
  virtual void initialize(Sensor *parent, Filter *next);

  void input(float value);

  void output(float value);

 protected:
  friend Sensor;

  Filter *next_{nullptr};
  Sensor *parent_{nullptr};
};

/** Base class for filters that use a sliding window of values.
 *
 * Uses a ring buffer to efficiently maintain a fixed-size sliding window without
 * reallocations or pop_front() overhead. Eliminates deque fragmentation issues.
 */
class SlidingWindowFilter : public Filter {
 public:
  SlidingWindowFilter(uint16_t window_size, uint16_t send_every, uint16_t send_first_at);

  optional<float> new_value(float value) final;

 protected:
  /// Called by new_value() to compute the filtered result from the current window
  virtual float compute_result() = 0;

  /// Sliding window ring buffer - automatically overwrites oldest values when full
  FixedRingBuffer<float> window_;
  uint16_t send_every_;  ///< Send result every N values
  uint16_t send_at_;     ///< Counter for send_every
};

/** Base class for Min/Max filters.
 *
 * Provides a templated helper to find extremum values efficiently.
 */
class MinMaxFilter : public SlidingWindowFilter {
 public:
  using SlidingWindowFilter::SlidingWindowFilter;

 protected:
  /// Helper to find min or max value in window, skipping NaN values
  /// Usage: find_extremum_<std::less<float>>() for min, find_extremum_<std::greater<float>>() for max
  template<typename Compare> float find_extremum_() {
    float result = NAN;
    Compare comp;
    for (float v : this->window_) {
      if (!std::isnan(v)) {
        result = std::isnan(result) ? v : (comp(v, result) ? v : result);
      }
    }
    return result;
  }
};

/** Base class for filters that need a sorted window (Median, Quantile).
 *
 * Extends SlidingWindowFilter to provide a helper that filters out NaN values.
 * Derived classes use std::nth_element for efficient partial sorting.
 */
class SortedWindowFilter : public SlidingWindowFilter {
 public:
  using SlidingWindowFilter::SlidingWindowFilter;

 protected:
  /// Helper to get non-NaN values from the window (not sorted - caller will use nth_element)
  /// Returns empty FixedVector if all values are NaN
  FixedVector<float> get_window_values_();
};

/** Simple quantile filter.
 *
 * Takes the quantile of the last <window_size> values and pushes it out every <send_every>.
 */
class QuantileFilter : public SortedWindowFilter {
 public:
  /** Construct a QuantileFilter.
   *
   * @param window_size The number of values that should be used in quantile calculation.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   * @param quantile float 0..1 to pick the requested quantile. Defaults to 0.9.
   */
  explicit QuantileFilter(size_t window_size, size_t send_every, size_t send_first_at, float quantile);

  void set_quantile(float quantile) { this->quantile_ = quantile; }

 protected:
  float compute_result() override;
  float quantile_;
};

/** Simple median filter.
 *
 * Takes the median of the last <window_size> values and pushes it out every <send_every>.
 */
class MedianFilter : public SortedWindowFilter {
 public:
  /** Construct a MedianFilter.
   *
   * @param window_size The number of values that should be used in median calculation.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  using SortedWindowFilter::SortedWindowFilter;

 protected:
  float compute_result() override;
};

/** Simple skip filter.
 *
 * Skips the first N values, then passes everything else.
 */
class SkipInitialFilter : public Filter {
 public:
  /** Construct a SkipInitialFilter.
   *
   * @param num_to_ignore How many values to ignore before the filter becomes a no-op.
   */
  explicit SkipInitialFilter(size_t num_to_ignore);

  optional<float> new_value(float value) override;

 protected:
  size_t num_to_ignore_;
};

/** Simple min filter.
 *
 * Takes the min of the last <window_size> values and pushes it out every <send_every>.
 */
class MinFilter : public MinMaxFilter {
 public:
  /** Construct a MinFilter.
   *
   * @param window_size The number of values that the min should be returned from.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  using MinMaxFilter::MinMaxFilter;

 protected:
  float compute_result() override;
};

/** Simple max filter.
 *
 * Takes the max of the last <window_size> values and pushes it out every <send_every>.
 */
class MaxFilter : public MinMaxFilter {
 public:
  /** Construct a MaxFilter.
   *
   * @param window_size The number of values that the max should be returned from.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  using MinMaxFilter::MinMaxFilter;

 protected:
  float compute_result() override;
};

/** Simple sliding window moving average filter.
 *
 * Essentially just takes takes the average of the last window_size values and pushes them out
 * every send_every.
 */
class SlidingWindowMovingAverageFilter : public SlidingWindowFilter {
 public:
  /** Construct a SlidingWindowMovingAverageFilter.
   *
   * @param window_size The number of values that should be averaged.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  using SlidingWindowFilter::SlidingWindowFilter;

 protected:
  float compute_result() override;
};

/** Simple exponential moving average filter.
 *
 * Essentially just takes the average of the last few values using exponentially decaying weights.
 * Use alpha to adjust decay rate.
 */
class ExponentialMovingAverageFilter : public Filter {
 public:
  ExponentialMovingAverageFilter(float alpha, uint16_t send_every, uint16_t send_first_at);

  optional<float> new_value(float value) override;

  void set_send_every(uint16_t send_every);
  void set_alpha(float alpha);

 protected:
  float accumulator_{NAN};
  float alpha_;
  uint16_t send_every_;
  uint16_t send_at_;
  bool first_value_{true};
};

/** Simple throttle average filter.
 *
 * It takes the average of all the values received in a period of time.
 */
class ThrottleAverageFilter : public Filter {
 public:
  explicit ThrottleAverageFilter(uint32_t time_period);

  void initialize(Sensor *parent, Filter *next) override;

  optional<float> new_value(float value) override;

 protected:
  float sum_{0.0f};
  uint32_t time_period_;
  // Sample count packed with NaN-seen flag in a single 32-bit word.
  // n_ is bounded by YAML cap on time_period_ (24 h) × max plausible source
  // rate (1 kHz) = 86.4M ≪ 2^31, so 31 bits has 25x headroom.
  uint32_t n_ : 31 {0};
  uint32_t have_nan_ : 1 {0};
};

using lambda_filter_t = std::function<optional<float>(float)>;

/** This class allows for creation of simple template filters.
 *
 * The constructor accepts a lambda of the form float -> optional<float>.
 * It will be called with each new value in the filter chain and returns the modified
 * value that shall be passed down the filter chain. Returning an empty Optional
 * means that the value shall be discarded.
 */
class LambdaFilter : public Filter {
 public:
  explicit LambdaFilter(lambda_filter_t lambda_filter);

  optional<float> new_value(float value) override;

  const lambda_filter_t &get_lambda_filter() const;
  void set_lambda_filter(const lambda_filter_t &lambda_filter);

 protected:
  lambda_filter_t lambda_filter_;
};

/** Optimized lambda filter for stateless lambdas (no capture).
 *
 * Uses function pointer instead of std::function to reduce memory overhead.
 * Memory: 4 bytes (function pointer on 32-bit) vs 32 bytes (std::function).
 */
class StatelessLambdaFilter : public Filter {
 public:
  explicit StatelessLambdaFilter(optional<float> (*lambda_filter)(float)) : lambda_filter_(lambda_filter) {}

  optional<float> new_value(float value) override { return this->lambda_filter_(value); }

 protected:
  optional<float> (*lambda_filter_)(float);
};

/// A simple filter that adds `offset` to each value it receives.
class OffsetFilter : public Filter {
 public:
  explicit OffsetFilter(TemplatableFn<float> offset);

  optional<float> new_value(float value) override;

 protected:
  TemplatableFn<float> offset_;
};

/// A simple filter that multiplies to each value it receives by `multiplier`.
class MultiplyFilter : public Filter {
 public:
  explicit MultiplyFilter(TemplatableFn<float> multiplier);
  optional<float> new_value(float value) override;

 protected:
  TemplatableFn<float> multiplier_;
};

/// Non-template helper for value matching (implementation in filter.cpp)
bool value_list_matches_any(Sensor *parent, float sensor_value, const TemplatableFn<float> *values, size_t count);

/** Base class for filters that compare sensor values against a fixed list of configured values.
 *
 * Templated on N (the number of values) so the list is stored inline in a std::array,
 * avoiding heap allocation and the overhead of FixedVector.
 *
 * @tparam N Number of values in the filter list, set by code generation to match
 *           the exact number of values configured in YAML.
 */
template<size_t N> class ValueListFilter : public Filter {
 protected:
  explicit ValueListFilter(std::initializer_list<TemplatableFn<float>> values) {
    init_array_from(this->values_, values);
  }

  /// Check if sensor value matches any configured value (with accuracy rounding)
  bool value_matches_any_(float sensor_value) {
    return value_list_matches_any(this->parent_, sensor_value, this->values_.data(), N);
  }

  std::array<TemplatableFn<float>, N> values_{};
};

/// A simple filter that only forwards the filter chain if it doesn't receive `value_to_filter_out`.
template<size_t N> class FilterOutValueFilter : public ValueListFilter<N> {
 public:
  explicit FilterOutValueFilter(std::initializer_list<TemplatableFn<float>> values_to_filter_out)
      : ValueListFilter<N>(values_to_filter_out) {}

  optional<float> new_value(float value) override {
    if (this->value_matches_any_(value))
      return {};   // Filter out
    return value;  // Pass through
  }
};

class ThrottleFilter : public Filter {
 public:
  explicit ThrottleFilter(uint32_t min_time_between_inputs);

  optional<float> new_value(float value) override;

 protected:
  uint32_t last_input_{0};
  uint32_t min_time_between_inputs_;
};

/// Non-template helper for ThrottleWithPriorityFilter (implementation in filter.cpp)
optional<float> throttle_with_priority_new_value(Sensor *parent, float value, const TemplatableFn<float> *values,
                                                 size_t count, uint32_t &last_input, uint32_t min_time_between_inputs);

/// Same as 'throttle' but will immediately publish values contained in `value_to_prioritize`.
template<size_t N> class ThrottleWithPriorityFilter : public ValueListFilter<N> {
 public:
  explicit ThrottleWithPriorityFilter(uint32_t min_time_between_inputs,
                                      std::initializer_list<TemplatableFn<float>> prioritized_values)
      : ValueListFilter<N>(prioritized_values), min_time_between_inputs_(min_time_between_inputs) {}

  optional<float> new_value(float value) override {
    return throttle_with_priority_new_value(this->parent_, value, this->values_.data(), N, this->last_input_,
                                            this->min_time_between_inputs_);
  }

 protected:
  uint32_t last_input_{0};
  uint32_t min_time_between_inputs_;
};

/// Specialization of ThrottleWithPriorityFilter for the common "prioritize NaN"
/// case: skips the TemplatableFn<float> array + lambda and inlines the check.
class ThrottleWithPriorityNanFilter : public Filter {
 public:
  explicit ThrottleWithPriorityNanFilter(uint32_t min_time_between_inputs);

  optional<float> new_value(float value) override;

 protected:
  uint32_t last_input_{0};
  uint32_t min_time_between_inputs_;
};

// Base class for timeout filters - contains common loop logic.
//
// Why this intentionally inherits Component (and does NOT use the self-keyed
// `App.scheduler.set_timeout(this, ...)` pattern that the other Filter classes
// migrated to):
//
// Timeout filters re-arm on every input, so on devices with many sensors
// using timeout filters (e.g. multi-LD2450 boards) every armed filter would
// require a live SchedulerItem in RAM at the same time. A SchedulerItem is
// substantially larger than the Component bookkeeping bytes carried by this
// class, so paying the Component cost per filter (one-time, BSS) is cheaper
// than paying for a SchedulerItem per filter (live, while armed). #11922
// is the original symptom and switchover to the loop-based design; #16173
// attempted to migrate this onto the scheduler and was closed for exactly
// this reason — even if the scheduler pool were unbounded, RAM per armed
// filter would still be dominated by the SchedulerItem itself, not by
// anything we can shrink in the scheduler.
//
// The loop-based design has additional advantages on top of the RAM win:
// `enable_loop()` / `disable_loop()` partitions the cost away when no
// timeout is armed; while armed, work is a single timestamp compare per
// active filter, with no per-input scheduler cancel/insert path.
//
// Don't try to migrate this class onto the self-keyed scheduler. The math
// doesn't work — at scale, this design is the smaller one.
class TimeoutFilterBase : public Filter, public Component {
 public:
  void loop() override;
  float get_setup_priority() const override;

 protected:
  explicit TimeoutFilterBase(uint32_t time_period) : time_period_(time_period) { this->disable_loop(); }
  virtual float get_output_value() = 0;

  uint32_t time_period_;            // 4 bytes (timeout duration in ms)
  uint32_t timeout_start_time_{0};  // 4 bytes (when the timeout was started)
  // Total base: 8 bytes
};

// Timeout filter for "last" mode - outputs the last received value after timeout
class TimeoutFilterLast : public TimeoutFilterBase {
 public:
  explicit TimeoutFilterLast(uint32_t time_period) : TimeoutFilterBase(time_period) {}

  optional<float> new_value(float value) override;

 protected:
  float get_output_value() override { return this->pending_value_; }
  float pending_value_{0};  // 4 bytes (value to output when timeout fires)
  // Total: 8 (base) + 4 = 12 bytes + vtable ptr + Component overhead
};

// Timeout filter with configured value - evaluates TemplatableValue after timeout
class TimeoutFilterConfigured : public TimeoutFilterBase {
 public:
  explicit TimeoutFilterConfigured(uint32_t time_period, const TemplatableFn<float> &new_value)
      : TimeoutFilterBase(time_period), value_(new_value) {}

  optional<float> new_value(float value) override;

 protected:
  float get_output_value() override { return this->value_.value(); }
  TemplatableFn<float> value_;  // 4 bytes (configured output value, can be lambda)
  // Total: 8 (base) + 4 = 12 bytes + vtable ptr + Component overhead
};

class DebounceFilter : public Filter {
 public:
  explicit DebounceFilter(uint32_t time_period);

  optional<float> new_value(float value) override;

 protected:
  uint32_t time_period_;
};

class HeartbeatFilter : public Filter {
 public:
  explicit HeartbeatFilter(uint32_t time_period);

  void initialize(Sensor *parent, Filter *next) override;
  optional<float> new_value(float value) override;

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }

 protected:
  uint32_t time_period_;
  float last_input_;
  bool has_value_{false};
  bool optimistic_{false};
};

class DeltaFilter : public Filter {
 public:
  explicit DeltaFilter(float min_a0, float min_a1, float max_a0, float max_a1);

  void set_baseline(float (*fn)(float));

  optional<float> new_value(float value) override;

 protected:
  // These values represent linear equations for the min and max values but in practice only one of a0 and a1 will be
  // non-zero Each limit is calculated as fabs(a0 + value * a1)

  float min_a0_, min_a1_, max_a0_, max_a1_;
  // default baseline is the previous value
  float (*baseline_)(float) = [](float last_value) { return last_value; };

  float last_value_{NAN};
};

/// Non-template helpers for OrFilter (implementation in filter.cpp)
void or_filter_initialize(Filter **filters, size_t count, Sensor *parent, Filter *phi);
optional<float> or_filter_new_value(Filter **filters, size_t count, float value, bool &has_value);

/// N is set by code generation to match the exact number of filters configured in YAML.
template<size_t N> class OrFilter : public Filter {
 public:
  explicit OrFilter(std::initializer_list<Filter *> filters) { init_array_from(this->filters_, filters); }

  void initialize(Sensor *parent, Filter *next) override {
    Filter::initialize(parent, next);
    or_filter_initialize(this->filters_.data(), N, parent, &this->phi_);
  }

  optional<float> new_value(float value) override {
    return or_filter_new_value(this->filters_.data(), N, value, this->has_value_);
  }

 protected:
  class PhiNode : public Filter {
   public:
    PhiNode(OrFilter *or_parent) : or_parent_(or_parent) {}
    optional<float> new_value(float value) override {
      if (!this->or_parent_->has_value_) {
        this->or_parent_->output(value);
        this->or_parent_->has_value_ = true;
      }
      return {};
    }

   protected:
    OrFilter *or_parent_;
  };

  std::array<Filter *, N> filters_{};
  PhiNode phi_{this};
  bool has_value_{false};
};

/// Non-template helper for linear calibration (implementation in filter.cpp)
optional<float> calibrate_linear_compute(const std::array<float, 3> *functions, size_t count, float value);

/// N is set by code generation to match the exact number of calibration segments.
template<size_t N> class CalibrateLinearFilter : public Filter {
 public:
  explicit CalibrateLinearFilter(std::initializer_list<std::array<float, 3>> linear_functions) {
    init_array_from(this->linear_functions_, linear_functions);
  }
  optional<float> new_value(float value) override {
    return calibrate_linear_compute(this->linear_functions_.data(), N, value);
  }

 protected:
  std::array<std::array<float, 3>, N> linear_functions_{};
};

/// Non-template helper for polynomial calibration (implementation in filter.cpp)
optional<float> calibrate_polynomial_compute(const float *coefficients, size_t count, float value);

/// N is set by code generation to match the exact number of polynomial coefficients.
template<size_t N> class CalibratePolynomialFilter : public Filter {
 public:
  explicit CalibratePolynomialFilter(std::initializer_list<float> coefficients) {
    init_array_from(this->coefficients_, coefficients);
  }
  optional<float> new_value(float value) override {
    return calibrate_polynomial_compute(this->coefficients_.data(), N, value);
  }

 protected:
  std::array<float, N> coefficients_{};
};

class ClampFilter : public Filter {
 public:
  ClampFilter(float min, float max, bool ignore_out_of_range);
  optional<float> new_value(float value) override;

 protected:
  float min_{NAN};
  float max_{NAN};
  bool ignore_out_of_range_;
};

class RoundFilter : public Filter {
 public:
  explicit RoundFilter(uint8_t precision);
  optional<float> new_value(float value) override;

 protected:
  uint8_t precision_;
};

class RoundMultipleFilter : public Filter {
 public:
  explicit RoundMultipleFilter(float multiple);
  optional<float> new_value(float value) override;

 protected:
  float multiple_;
};

template<uint8_t Digits> class RoundSignificantDigitsFilter : public Filter {
 public:
  optional<float> new_value(float value) override {
    if (std::isfinite(value)) {
      if (value == 0.0f)
        return 0.0f;
      float factor = pow10_int(Digits - 1 - ilog10(value));
      return roundf(value * factor) / factor;
    }
    return value;
  }
};

class ToNTCResistanceFilter : public Filter {
 public:
  ToNTCResistanceFilter(double a, double b, double c) : a_(a), b_(b), c_(c) {}
  optional<float> new_value(float value) override;

 protected:
  double a_;
  double b_;
  double c_;
};

class ToNTCTemperatureFilter : public Filter {
 public:
  ToNTCTemperatureFilter(double a, double b, double c) : a_(a), b_(b), c_(c) {}
  optional<float> new_value(float value) override;

 protected:
  double a_;
  double b_;
  double c_;
};

/** Base class for streaming filters (batch windows where window_size == send_every).
 *
 * When window_size equals send_every, we don't need a sliding window.
 * This base class handles the common batching logic.
 */
class StreamingFilter : public Filter {
 public:
  StreamingFilter(uint16_t window_size, uint16_t send_first_at);

  optional<float> new_value(float value) final;

 protected:
  /// Called by new_value() to process each value in the batch
  virtual void process_value(float value) = 0;

  /// Called by new_value() to compute the result after collecting window_size values
  virtual float compute_batch_result() = 0;

  /// Called by new_value() to reset internal state after sending a result
  virtual void reset_batch() = 0;

  uint16_t window_size_;
  uint16_t count_{0};
  uint16_t send_first_at_;
  bool first_send_{true};
};

/** Streaming min filter for batch windows (window_size == send_every).
 *
 * Uses O(1) memory instead of O(n) by tracking only the minimum value.
 */
class StreamingMinFilter : public StreamingFilter {
 public:
  using StreamingFilter::StreamingFilter;

 protected:
  void process_value(float value) override;
  float compute_batch_result() override;
  void reset_batch() override;

  float current_min_{NAN};
};

/** Streaming max filter for batch windows (window_size == send_every).
 *
 * Uses O(1) memory instead of O(n) by tracking only the maximum value.
 */
class StreamingMaxFilter : public StreamingFilter {
 public:
  using StreamingFilter::StreamingFilter;

 protected:
  void process_value(float value) override;
  float compute_batch_result() override;
  void reset_batch() override;

  float current_max_{NAN};
};

/** Streaming moving average filter for batch windows (window_size == send_every).
 *
 * Uses O(1) memory instead of O(n) by tracking only sum and count.
 */
class StreamingMovingAverageFilter : public StreamingFilter {
 public:
  using StreamingFilter::StreamingFilter;

 protected:
  void process_value(float value) override;
  float compute_batch_result() override;
  void reset_batch() override;

  float sum_{0.0f};
  size_t valid_count_{0};
};

}  // namespace esphome::sensor

#endif  // USE_SENSOR_FILTER
