#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF)

#include <cmath>
#include <cstdint>
#include <limits>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace sendspin {

struct TimeElement {
  int64_t last_update{0};
  double offset{0};
  double drift{0};
};

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
/// Thread-safe access to the current time transformation is provided via FreeRTOS queues.
class SendspinTimeFilter {
 public:
  SendspinTimeFilter(double process_std_dev, double forget_factor);
  ~SendspinTimeFilter();

  /// @brief Processes a new time synchronization measurement through the Kalman filter.
  ///
  /// Updates the filter's offset and drift estimates using a two-stage Kalman filter algorithm: predict based on the
  /// drift model then correct using the new measurement. The measurement uncertainty is derived from the network
  /// round-trip delay.
  ///
  /// @note Thread-safe when called concurrently with compute_server_time() or compute_client_time().
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
  /// @note Not thread-safe when called concurrently with compute_client_time().
  /// @param client_time Client timestamp in microseconds.
  /// @return Equivalent server timestamp in microseconds.
  int64_t compute_server_time(int64_t client_time);

  /// @brief Converts a server timestamp to the equivalent client timestamp.
  ///
  /// Inverts the time transformation to convert from server time domain to client time domain. Accounts for both offset
  /// and drift effects in the inverse transformation.
  ///
  /// @note Not thread-safe when called concurrently with compute_server_time().
  /// @param server_time Server timestamp in microseconds.
  /// @return Equivalent client timestamp in microseconds.
  int64_t compute_client_time(int64_t server_time);

  /// @brief Resets the filter to its initial uninitialized state.
  ///
  /// Clears all state estimates and resets covariances to initial values. The filter will require new measurements to
  /// re-establish synchronization.
  void reset();

  int64_t get_error() const { return std::round(sqrt(this->offset_covariance_)); }
  int64_t get_covariance() const { return std::round(this->offset_covariance_); }

 protected:
  int64_t last_update_{0};
  uint8_t count_{0};

  double offset_{0.0};
  double drift_{0.0};

  double offset_covariance_{std::numeric_limits<double>::infinity()};
  double offset_drift_covariance_{0.0};
  double drift_covariance_{0.0};

  double process_variance_;
  double forget_variance_factor_;

  QueueHandle_t time_element_queue_;
  TimeElement current_time_element_;
};

}  // namespace sendspin
}  // namespace esphome

#endif
