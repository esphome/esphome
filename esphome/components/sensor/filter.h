#pragma once

#include <queue>
#include <utility>
#include <vector>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sensor {

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
  SlidingWindowFilter(size_t window_size, size_t send_every, size_t send_first_at);

  optional<float> new_value(float value) final;

 protected:
  /// Called by new_value() to compute the filtered result from the current window
  virtual float compute_result() = 0;

  /// Access the sliding window values (ring buffer implementation)
  /// Use: for (size_t i = 0; i < window_count_; i++) { float val = window_[i]; }
  FixedVector<float> window_;
  size_t window_head_{0};   ///< Index where next value will be written
  size_t window_count_{0};  ///< Number of valid values in window (0 to window_size_)
  size_t window_size_;      ///< Maximum window size
  size_t send_every_;       ///< Send result every N values
  size_t send_at_;          ///< Counter for send_every
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
    for (size_t i = 0; i < this->window_count_; i++) {
      float v = this->window_[i];
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
  ExponentialMovingAverageFilter(float alpha, size_t send_every, size_t send_first_at);

  optional<float> new_value(float value) override;

  void set_send_every(size_t send_every);
  void set_alpha(float alpha);

 protected:
  float accumulator_{NAN};
  float alpha_;
  size_t send_every_;
  size_t send_at_;
  bool first_value_{true};
};

/** Simple throttle average filter.
 *
 * It takes the average of all the values received in a period of time.
 */
class ThrottleAverageFilter : public Filter, public Component {
 public:
  explicit ThrottleAverageFilter(uint32_t time_period);

  void setup() override;

  optional<float> new_value(float value) override;

  float get_setup_priority() const override;

 protected:
  float sum_{0.0f};
  unsigned int n_{0};
  uint32_t time_period_;
  bool have_nan_{false};
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
  explicit OffsetFilter(TemplatableValue<float> offset);

  optional<float> new_value(float value) override;

 protected:
  TemplatableValue<float> offset_;
};

/// A simple filter that multiplies to each value it receives by `multiplier`.
class MultiplyFilter : public Filter {
 public:
  explicit MultiplyFilter(TemplatableValue<float> multiplier);
  optional<float> new_value(float value) override;

 protected:
  TemplatableValue<float> multiplier_;
};

/** Base class for filters that compare sensor values against a list of configured values.
 *
 * This base class provides common functionality for filters that need to check if a sensor
 * value matches any value in a configured list, with proper handling of NaN values and
 * accuracy-based rounding for comparisons.
 */
class ValueListFilter : public Filter {
 protected:
  explicit ValueListFilter(std::initializer_list<TemplatableValue<float>> values);

  /// Check if sensor value matches any configured value (with accuracy rounding)
  bool value_matches_any_(float sensor_value);

  FixedVector<TemplatableValue<float>> values_;
};

/// A simple filter that only forwards the filter chain if it doesn't receive `value_to_filter_out`.
class FilterOutValueFilter : public ValueListFilter {
 public:
  explicit FilterOutValueFilter(std::initializer_list<TemplatableValue<float>> values_to_filter_out);

  optional<float> new_value(float value) override;
};

class ThrottleFilter : public Filter {
 public:
  explicit ThrottleFilter(uint32_t min_time_between_inputs);

  optional<float> new_value(float value) override;

 protected:
  uint32_t last_input_{0};
  uint32_t min_time_between_inputs_;
};

/// Same as 'throttle' but will immediately publish values contained in `value_to_prioritize`.
class ThrottleWithPriorityFilter : public ValueListFilter {
 public:
  explicit ThrottleWithPriorityFilter(uint32_t min_time_between_inputs,
                                      std::initializer_list<TemplatableValue<float>> prioritized_values);

  optional<float> new_value(float value) override;

 protected:
  uint32_t last_input_{0};
  uint32_t min_time_between_inputs_;
};

// Base class for timeout filters - contains common loop logic
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
  explicit TimeoutFilterConfigured(uint32_t time_period, const TemplatableValue<float> &new_value)
      : TimeoutFilterBase(time_period), value_(new_value) {}

  optional<float> new_value(float value) override;

 protected:
  float get_output_value() override { return this->value_.value(); }
  TemplatableValue<float> value_;  // 16 bytes (configured output value, can be lambda)
  // Total: 8 (base) + 16 = 24 bytes + vtable ptr + Component overhead
};

class DebounceFilter : public Filter, public Component {
 public:
  explicit DebounceFilter(uint32_t time_period);

  optional<float> new_value(float value) override;

  float get_setup_priority() const override;

 protected:
  uint32_t time_period_;
};

class HeartbeatFilter : public Filter, public Component {
 public:
  explicit HeartbeatFilter(uint32_t time_period);

  void setup() override;
  optional<float> new_value(float value) override;
  float get_setup_priority() const override;

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }

 protected:
  uint32_t time_period_;
  float last_input_;
  bool has_value_{false};
  bool optimistic_{false};
};

class DeltaFilter : public Filter {
 public:
  explicit DeltaFilter(float delta, bool percentage_mode);

  optional<float> new_value(float value) override;

 protected:
  float delta_;
  float current_delta_;
  float last_value_{NAN};
  bool percentage_mode_;
};

class OrFilter : public Filter {
 public:
  explicit OrFilter(std::initializer_list<Filter *> filters);

  void initialize(Sensor *parent, Filter *next) override;

  optional<float> new_value(float value) override;

 protected:
  class PhiNode : public Filter {
   public:
    PhiNode(OrFilter *or_parent);
    optional<float> new_value(float value) override;

   protected:
    OrFilter *or_parent_;
  };

  FixedVector<Filter *> filters_;
  PhiNode phi_;
  bool has_value_{false};
};

class CalibrateLinearFilter : public Filter {
 public:
  explicit CalibrateLinearFilter(std::initializer_list<std::array<float, 3>> linear_functions);
  optional<float> new_value(float value) override;

 protected:
  FixedVector<std::array<float, 3>> linear_functions_;
};

class CalibratePolynomialFilter : public Filter {
 public:
  explicit CalibratePolynomialFilter(std::initializer_list<float> coefficients);
  optional<float> new_value(float value) override;

 protected:
  FixedVector<float> coefficients_;
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
  StreamingFilter(size_t window_size, size_t send_first_at);

  optional<float> new_value(float value) final;

 protected:
  /// Called by new_value() to process each value in the batch
  virtual void process_value(float value) = 0;

  /// Called by new_value() to compute the result after collecting window_size values
  virtual float compute_batch_result() = 0;

  /// Called by new_value() to reset internal state after sending a result
  virtual void reset_batch() = 0;

  size_t window_size_;
  size_t count_{0};
  size_t send_first_at_;
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

}  // namespace sensor
}  // namespace esphome
