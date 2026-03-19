// Copyright 2025 Sendspin Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"

#include <cstdint>

namespace esphome::sendspin {

// Time filter tuning constants
static constexpr double TIME_FILTER_PROCESS_STD_DEV = 0.0;
static constexpr double TIME_FILTER_DRIFT_PROCESS_STD_DEV = 5e-11;
static constexpr double TIME_FILTER_FORGET_FACTOR = 1.1;
static constexpr double TIME_FILTER_ADAPTIVE_CUTOFF = 2.0;
static constexpr uint8_t TIME_FILTER_MIN_SAMPLES = 100;
static constexpr double TIME_FILTER_DRIFT_SIGNIFICANCE_THRESHOLD = 2.0;

/// @brief Two-dimensional Kalman filter for NTP-style time synchronization between client and server.
///
/// This class implements a time synchronization filter that tracks both the timestamp offset and clock drift rate
/// between a client and server. It processes measurements obtained with NTP-style time messages that contain round-trip
/// timing information to optimally estimate the time relationship while accounting for network latency uncertainty.
///
/// The filter maintains a 2D state vector [offset, drift] with associated covariance matrix to track estimation
/// uncertainty. An adaptive forgetting factor helps the filter recover quickly from network disruptions or server clock
/// adjustments.
///
/// All computations use double precision arithmetic to maintain microsecond-level accuracy over extended periods.
/// Thread-safe access to the current time transformation is provided via std::mutex.
class SendspinTimeFilter {
 public:
  /// @brief Constructs a Kalman filter for time synchronization.
  ///
  /// @param process_std_dev Standard deviation of the offset process noise in microseconds, modeling clock jitter.
  /// @param drift_process_std_dev Standard deviation of the drift process noise in microseconds/second, modeling
  ///                              frequency wander.
  /// @param forget_factor Forgetting factor (>1) applied to covariances when large residuals are detected.
  ///                      Higher values enable faster recovery from disruptions but may reduce stability.
  /// @param adaptive_cutoff Fraction of max_error (0-1) that triggers adaptive forgetting. Default 0.75.
  ///                        When residual > adaptive_cutoff * max_error, forgetting is applied.
  /// @param min_samples Minimum number of samples before adaptive forgetting is enabled. Default 100.
  ///                    Building sufficient history before enabling forgetting improves stability.
  /// @param drift_significance_threshold SNR threshold for applying drift compensation in time conversions.
  /// Default 2.0.
  ///                                     Drift is only used when drift² > threshold² * drift_covariance, ensuring
  ///                                     the drift estimate is statistically significant before applying corrections.
  SendspinTimeFilter(double process_std_dev, double drift_process_std_dev, double forget_factor,
                     double adaptive_cutoff = 0.75, uint8_t min_samples = 100,
                     double drift_significance_threshold = 2.0);

  /// @brief Processes a new time synchronization measurement through the Kalman filter.
  ///
  /// Updates the filter's offset and drift estimates using a two-stage Kalman filter algorithm: predict based on the
  /// drift model then correct using the new measurement. The measurement uncertainty is derived from the network
  /// round-trip delay.
  ///
  /// @param measurement Computed offset from NTP-style exchange: ((T2-T1)+(T3-T4))/2 in microseconds.
  /// @param max_error Half the round-trip delay: ((T4-T1)-(T3-T2))/2, representing maximum measurement uncertainty in
  ///                  microseconds.
  /// @param time_added Client timestamp when this measurement was taken in microseconds.
  void update(int64_t measurement, int64_t max_error, int64_t time_added);

  /// @brief Converts a client timestamp to the equivalent server timestamp.
  ///
  /// Applies the current offset and drift compensation to transform from client time domain to server time domain. The
  /// transformation accounts for both static offset and dynamic drift accumulated since the last filter update.
  ///
  /// @param client_time Client timestamp in microseconds.
  /// @return Equivalent server timestamp in microseconds.
  int64_t compute_server_time(int64_t client_time) const;

  /// @brief Converts a server timestamp to the equivalent client timestamp.
  ///
  /// Inverts the time transformation to convert from server time domain to client time domain. Accounts for both offset
  /// and drift effects in the inverse transformation.
  ///
  /// @param server_time Server timestamp in microseconds.
  /// @return Equivalent client timestamp in microseconds.
  int64_t compute_client_time(int64_t server_time) const;

  /// @brief Resets the filter to its initial uninitialized state.
  ///
  /// Clears all state estimates and resets covariances to initial values. The filter will require new measurements to
  /// re-establish synchronization.
  void reset();

  /// @brief Returns the estimated standard deviation of the offset in microseconds.
  ///
  /// Provides a measure of the current synchronization accuracy by computing the square root of the offset covariance.
  /// Smaller values indicate higher confidence in the time synchronization.
  ///
  /// @return Standard deviation of the offset estimate in microseconds.
  int64_t get_error() const;

  /// @brief Returns true if the filter has received at least one measurement.
  /// @return True if the filter has been updated with at least one time measurement.
  bool has_update() const;

  /// @brief Returns the offset variance in microseconds squared.
  ///
  /// Provides the raw variance value from the Kalman filter's covariance matrix. This represents the statistical
  /// uncertainty in the offset estimate.
  ///
  /// @return Variance of the offset estimate in microseconds squared.
  int64_t get_covariance() const;

 protected:
  int64_t last_update_;

  double offset_;
  double drift_;

  double offset_covariance_;
  double offset_drift_covariance_;
  double drift_covariance_;

  const double process_variance_;
  const double drift_process_variance_;
  const double forget_variance_factor_;
  const double adaptive_forgetting_cutoff_;
  const double drift_significance_threshold_squared_;

  mutable esphome::Mutex state_mutex_;

  bool use_drift_;
  uint8_t count_;
  const uint8_t min_samples_for_forgetting_;
};
}  // namespace esphome::sendspin

#endif  // USE_ESP32
