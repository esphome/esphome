#pragma once

#include <queue>
#include <utility>
#include <vector>
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

/** Simple quantile filter.
 *
 * Takes the quantile of the last <send_every> values and pushes it out every <send_every>.
 */
class QuantileFilter : public Filter {
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

  optional<float> new_value(float value) override;

  void set_send_every(size_t send_every);
  void set_window_size(size_t window_size);
  void set_quantile(float quantile);

 protected:
  std::deque<float> queue_;
  size_t send_every_;
  size_t send_at_;
  size_t window_size_;
  float quantile_;
};

/** Simple median filter.
 *
 * Takes the median of the last <send_every> values and pushes it out every <send_every>.
 */
class MedianFilter : public Filter {
 public:
  /** Construct a MedianFilter.
   *
   * @param window_size The number of values that should be used in median calculation.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  explicit MedianFilter(size_t window_size, size_t send_every, size_t send_first_at);

  optional<float> new_value(float value) override;

  void set_send_every(size_t send_every);
  void set_window_size(size_t window_size);

 protected:
  std::deque<float> queue_;
  size_t send_every_;
  size_t send_at_;
  size_t window_size_;
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
 * Takes the min of the last <send_every> values and pushes it out every <send_every>.
 */
class MinFilter : public Filter {
 public:
  /** Construct a MinFilter.
   *
   * @param window_size The number of values that the min should be returned from.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  explicit MinFilter(size_t window_size, size_t send_every, size_t send_first_at);

  optional<float> new_value(float value) override;

  void set_send_every(size_t send_every);
  void set_window_size(size_t window_size);

 protected:
  std::deque<float> queue_;
  size_t send_every_;
  size_t send_at_;
  size_t window_size_;
};

/** Simple max filter.
 *
 * Takes the max of the last <send_every> values and pushes it out every <send_every>.
 */
class MaxFilter : public Filter {
 public:
  /** Construct a MaxFilter.
   *
   * @param window_size The number of values that the max should be returned from.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  explicit MaxFilter(size_t window_size, size_t send_every, size_t send_first_at);

  optional<float> new_value(float value) override;

  void set_send_every(size_t send_every);
  void set_window_size(size_t window_size);

 protected:
  std::deque<float> queue_;
  size_t send_every_;
  size_t send_at_;
  size_t window_size_;
};

/** Simple sliding window moving average filter.
 *
 * Essentially just takes takes the average of the last window_size values and pushes them out
 * every send_every.
 */
class SlidingWindowMovingAverageFilter : public Filter {
 public:
  /** Construct a SlidingWindowMovingAverageFilter.
   *
   * @param window_size The number of values that should be averaged.
   * @param send_every After how many sensor values should a new one be pushed out.
   * @param send_first_at After how many values to forward the very first value. Defaults to the first value
   *   on startup being published on the first *raw* value, so with no filter applied. Must be less than or equal to
   *   send_every.
   */
  explicit SlidingWindowMovingAverageFilter(size_t window_size, size_t send_every, size_t send_first_at);

  optional<float> new_value(float value) override;

  void set_send_every(size_t send_every);
  void set_window_size(size_t window_size);

 protected:
  std::deque<float> queue_;
  size_t send_every_;
  size_t send_at_;
  size_t window_size_;
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
  bool first_value_{true};
  float accumulator_{NAN};
  size_t send_every_;
  size_t send_at_;
  float alpha_;
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
  uint32_t time_period_;
  float sum_{0.0f};
  unsigned int n_{0};
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

/// A simple filter that adds `offset` to each value it receives.
class OffsetFilter : public Filter {
 public:
  explicit OffsetFilter(float offset);

  optional<float> new_value(float value) override;

 protected:
  float offset_;
};

/// A simple filter that multiplies to each value it receives by `multiplier`.
class MultiplyFilter : public Filter {
 public:
  explicit MultiplyFilter(float multiplier);

  optional<float> new_value(float value) override;

 protected:
  float multiplier_;
};

/// A simple filter that only forwards the filter chain if it doesn't receive `value_to_filter_out`.
class FilterOutValueFilter : public Filter {
 public:
  explicit FilterOutValueFilter(float value_to_filter_out);

  optional<float> new_value(float value) override;

 protected:
  float value_to_filter_out_;
};

class ThrottleFilter : public Filter {
 public:
  explicit ThrottleFilter(uint32_t min_time_between_inputs);

  optional<float> new_value(float value) override;

 protected:
  uint32_t last_input_{0};
  uint32_t min_time_between_inputs_;
};

class TimeoutFilter : public Filter, public Component {
 public:
  explicit TimeoutFilter(uint32_t time_period, float new_value);
  void set_value(float new_value) { this->value_ = new_value; }

  optional<float> new_value(float value) override;

  float get_setup_priority() const override;

 protected:
  uint32_t time_period_;
  float value_;
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

 protected:
  uint32_t time_period_;
  float last_input_;
  bool has_value_{false};
};

class DeltaFilter : public Filter {
 public:
  explicit DeltaFilter(float delta, bool percentage_mode);

  optional<float> new_value(float value) override;

 protected:
  float delta_;
  float current_delta_;
  bool percentage_mode_;
  float last_value_{NAN};
};

class OrFilter : public Filter {
 public:
  explicit OrFilter(std::vector<Filter *> filters);

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

  std::vector<Filter *> filters_;
  PhiNode phi_;
};

class CalibrateLinearFilter : public Filter {
 public:
  CalibrateLinearFilter(std::vector<std::array<float, 3>> linear_functions)
      : linear_functions_(std::move(linear_functions)) {}
  optional<float> new_value(float value) override;

 protected:
  std::vector<std::array<float, 3>> linear_functions_;
};

class CalibratePolynomialFilter : public Filter {
 public:
  CalibratePolynomialFilter(std::vector<float> coefficients) : coefficients_(std::move(coefficients)) {}
  optional<float> new_value(float value) override;

 protected:
  std::vector<float> coefficients_;
};

class ClampFilter : public Filter {
 public:
  ClampFilter(float min, float max);
  optional<float> new_value(float value) override;

 protected:
  float min_{NAN};
  float max_{NAN};
};

}  // namespace sensor
}  // namespace esphome
