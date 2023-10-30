#include "aggregate.h"

#include <algorithm>  // necessary for std::min and std::max functions
#include <cstdint>    // necessary for uint32_t
#include <cmath>      // necessary for NaN

namespace esphome {
namespace statistics {

void Aggregate::add_measurement(double value, uint64_t duration, uint32_t timestamp, time_t unix_time) {
  if (!std::isnan(value)) {
    this->argmax_ = unix_time;
    this->argmin_ = unix_time;
    this->c2_ = 0.0;
    this->count_ = 1;
    this->m2_ = 0.0;
    this->max_ = value;
    this->mean_ = value;
    this->min_ = value;
    this->timestamp_m2_ = 0.0;
    this->timestamp_mean_ = 0.0;
    this->timestamp_reference_ = timestamp;
    this->duration_ = duration;
    this->duration_squared_ = duration * duration;
  }
}

void Aggregate::clear() {
  this->argmax_ = std::numeric_limits<time_t>::infinity() * (-1);
  this->argmin_ = std::numeric_limits<time_t>::infinity() * (-1);
  this->c2_ = NAN;
  this->count_ = 0;
  this->m2_ = NAN;
  this->max_ = std::numeric_limits<float>::infinity() * (-1);
  this->mean_ = NAN;
  this->min_ = std::numeric_limits<float>::infinity();
  this->timestamp_m2_ = NAN;
  this->timestamp_mean_ = NAN;
  this->timestamp_reference_ = 0;
  this->duration_ = 0;
  this->duration_squared_ = 0;
}

Aggregate &Aggregate::operator+=(const Aggregate &b) {
  // If either of the Aggregates is the identity, return the other Aggregate.
  if (b.get_count() == 0) {
    return *this;
  } else if (this->get_count() == 0) {
    *this = b;
    return *this;
  }

  Aggregate combined(this->statistics_calculation_config_);

  combined.count_ = this->get_count() + b.get_count();
  combined.duration_ = this->get_duration() + b.get_duration();
  combined.duration_squared_ = this->get_duration_squared() + b.get_duration_squared();

  // Combine max and argmax
  if (this->get_max() < b.get_max()) {
    combined.max_ = b.get_max();
    combined.argmax_ = b.get_argmax();
  } else if (this->get_max() > b.get_max()) {
    combined.max_ = this->get_max();
    combined.argmax_ = this->get_argmax();
  } else {  // the Aggregate's maximums are equal; use the more recent timestamp for argmax
    combined.max_ = this->get_max();
    combined.argmax_ = std::max(this->get_argmax(), b.get_argmax());  // use the more recent UTC Unix time
  }

  // Combine min and argmin
  if (this->get_min() < b.get_min()) {
    combined.min_ = this->get_min();
    combined.argmin_ = this->get_argmin();
  } else if (this->get_min() > b.get_min()) {
    combined.min_ = b.get_min();
    combined.argmin_ = b.get_argmin();
  } else {  // the Aggregate's minimums are equal; use the more recent timestamp for argmin
    combined.min_ = this->get_min();
    combined.argmin_ = std::max(this->get_argmin(), b.get_argmin());  // use the more recent UTC Unix time
  }

  double a_weight, b_weight, combined_weight;

  double a_timestamp_mean = this->get_timestamp_mean();
  double b_timestamp_mean = b.get_timestamp_mean();

  combined.timestamp_reference_ =
      normalize_timestamp_means(a_timestamp_mean, this->get_timestamp_reference(), this->get_count(), b_timestamp_mean,
                                b.get_timestamp_reference(), b.get_count());

  // If the averages are time-weighted, then use measurement durations.
  // Otherwise, use the Aggregates' counts as the weights.
  if (this->statistics_calculation_config_.weight_type == WEIGHT_TYPE_DURATION) {
    a_weight = static_cast<double>(this->get_duration());
    b_weight = static_cast<double>(b.get_duration());
    combined_weight = static_cast<double>(combined.duration_);
  } else {
    a_weight = static_cast<double>(this->get_count());
    b_weight = static_cast<double>(b.get_count());
    combined_weight = static_cast<double>(combined.count_);
  }

  double delta = b.get_mean() - this->get_mean();
  double delta_prime = delta * b_weight / combined_weight;

  double timestamp_delta = b_timestamp_mean - a_timestamp_mean;
  double timestamp_delta_prime = timestamp_delta * b_weight / combined_weight;

  // Compute the mean and timestamp mean.
  if ((b.get_count() < combined.count_ / 4) || (this->get_count() < combined.count_ / 4)) {
    // If either count is significantly smaller than the other, then use a variation of Welford's algorithm for speed.
    combined.mean_ = this->get_mean() + delta_prime;
    combined.timestamp_mean_ = a_timestamp_mean + timestamp_delta_prime;
  } else {
    // Otherwise, use a weighted average for numerical stability.
    combined.mean_ = this->get_mean() * (a_weight / combined_weight) + b.get_mean() * (b_weight / combined_weight);
    combined.timestamp_mean_ =
        a_timestamp_mean * (a_weight / combined_weight) + b_timestamp_mean * (b_weight / combined_weight);
  }

  // Compute M2 quantity for Welford's algorithm, which determines the variance.
  combined.m2_ = this->get_m2() + b.get_m2() + a_weight * delta * delta_prime;
  combined.timestamp_m2_ =
      this->get_timestamp_m2() + b.get_timestamp_m2() + a_weight * timestamp_delta * timestamp_delta_prime;

  // Compute C2 quantity for a variation of Welford's algorithm, which determines the covariance.
  combined.c2_ = this->get_c2() + b.get_c2() + a_weight * delta * timestamp_delta_prime;

  *this = combined;

  return *this;
}

Aggregate Aggregate::operator+(const Aggregate &b) const { return Aggregate(*this) += b; }

double Aggregate::compute_covariance() const {
  if (this->count_ > 1)
    return this->c2_ / this->denominator_();

  return NAN;
}

double Aggregate::compute_quadrature() const { return this->mean_ * static_cast<double>(this->duration_); }

double Aggregate::compute_std_dev() const { return std::sqrt(this->compute_variance()); }

double Aggregate::compute_trend() const {
  if (this->count_ > 1)
    return this->c2_ / this->timestamp_m2_;
  return NAN;
}

double Aggregate::compute_variance() const {
  if (this->count_ > 1)
    return this->m2_ / this->denominator_();

  return NAN;
}

//////////////////////
// Internal Methods //
//////////////////////

double Aggregate::denominator_() const {
  // Bessel's correction for non-time weighted samples (https://en.wikipedia.org/wiki/Bessel%27s_correction, accessed
  // June 2023)
  double denominator = this->count_ - 1;  // GROUP_TYPE_SAMPLE and WEIGHT_TYPE_SIMPLE

  if (this->statistics_calculation_config_.weight_type != WEIGHT_TYPE_DURATION) {
    if (this->statistics_calculation_config_.group_type == GROUP_TYPE_POPULATION)
      denominator = static_cast<double>(this->count_);  // GROUP_TYPE_POPULATION and WEIGHT_TYPE_SIMPLE
  } else {
    if (this->statistics_calculation_config_.group_type == GROUP_TYPE_SAMPLE) {
      // Reliability weights for weighted samples
      // (http://en.wikipedia.org/wiki/Weighted_arithmetic_mean#Weighted_sample_variance, accessed June 2023)
      denominator = static_cast<double>(this->duration_) -
                    static_cast<double>(this->duration_squared_) /
                        static_cast<double>(this->duration_);  // GROUP_TYPE_SAMPLE and WEIGHT_TYPE_DURATION
    } else if (this->statistics_calculation_config_.group_type == GROUP_TYPE_POPULATION)
      denominator = static_cast<double>(this->duration_);  // GROUP_TYPE_POPULATION and WEIGHT_TYPE_DURATION
  }

  return denominator;
}

double Aggregate::normalize_timestamp_means(double &a_mean, uint32_t a_timestamp_reference, size_t a_count,
                                            double &b_mean, uint32_t b_timestamp_reference, size_t b_count) {
  if (a_count == 0) {
    // a is null, so b is always the more recent timestamp; no adjustments are necessary
    return b_timestamp_reference;
  }
  if (b_count == 0) {
    // b is null, so a is always the more recent timestamp; no adjustments are necessary
    return a_timestamp_reference;
  }

  if (b_timestamp_reference == more_recent_timestamp(a_timestamp_reference, b_timestamp_reference)) {
    // b is the more recent timestamp, so normalize the a_mean using the b_timestamp

    uint32_t timestamp_delta = b_timestamp_reference - a_timestamp_reference;
    a_mean = a_mean - timestamp_delta;  // a_mean is now offset and normalized to b_timestamp_reference

    return b_timestamp_reference;  // both timestamps are in reference to b_timestamp_reference
  } else {
    // a is the more recent timestamp, so normalize the b_mean using the a_timestamp

    uint32_t timestamp_delta = a_timestamp_reference - b_timestamp_reference;
    b_mean = b_mean - timestamp_delta;  // b_mean is now offset and normalized to a_timestamp_reference

    return a_timestamp_reference;
  }
}

inline uint32_t Aggregate::more_recent_timestamp(const uint32_t a_timestamp, const uint32_t b_timestamp) {
  // Test the sign bit of the difference to see if the subtraction rolls over
  //  - this assumes the timestamps are not truly more than 2^31 ms apart, which is about 24.86 days
  //    (https://arduino.stackexchange.com/a/12591, accessed June 2023)
  if ((a_timestamp - b_timestamp) & 0x80000000)
    return b_timestamp;

  return a_timestamp;
}

}  // namespace statistics
}  // namespace esphome
