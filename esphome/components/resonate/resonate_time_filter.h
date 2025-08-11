#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF)

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace resonate {

struct TimeElement {
  int64_t last_update{0};
  double offset{0};
  double drift{0};
};

class ResonateTimeFilter {
  /* A helper class to convert the client's timestamps into the server's timestamps. It uses a Kalman filter to process
   * new values from time messages, taking into account the maximum error due to the network connection latency. The
   * Kalman filter works in two dimensions, estimating the client server timestamp offset and the rate of the drift of
   * the client's clock compared to the servers. Most computations are performed using ouble precision floating points,
   * which is necessary to handle working with microseconds. If the Kalman prediction is far off from a new update's
   * values, a forget factor is applied to force the filter to focus on the most recent updates.
   */
 public:
  ResonateTimeFilter(double process_std_dev, double forget_factor);

  void update(int64_t measurement, int64_t max_error, int64_t time_added);

  /// @brief Converts a client timestamp to a server timestamp.
  /// @param client_time The client's timestamp to convert.
  /// @return The equivalent server timestamp.
  int64_t compute_server_time(int64_t client_time);

  int64_t compute_client_time(int64_t server_time);

  /// @brief Resets the filter.
  void reset();

  int64_t get_error() { return std::round(sqrt(this->offset_covariance_)); }
  int64_t get_covariance() { return std::round(this->offset_covariance_); }

 protected:
  /// @brief Computes the client server timestamp offset, taking into account the drift rate
  /// @param timestamp Client timestamp.
  /// @return Client server timestamp offset
  int64_t compute_offset_(int64_t timestamp);

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

}  // namespace resonate
}  // namespace esphome

#endif
