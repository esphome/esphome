#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <cstdint>
#include <limits>

namespace esphome {
namespace sendspin {

class SendspinConnection;

/// @brief Result of a single SendspinTimeBurst::loop() call.
struct TimeBurstResult {
  bool sent;             ///< A time message was sent this call.
  bool burst_completed;  ///< The burst just finished (Kalman filter updated).
};

/// @brief Burst-based time synchronization helper for Sendspin.
///
/// Sends a rapid burst of time messages, picks the one with the smallest RTT (max_error),
/// and feeds only that best measurement to the Kalman filter. This reduces noise from
/// variable network latency by selecting the cleanest sample per burst.
class SendspinTimeBurst {
 public:
  /// @brief Drive the burst state machine. Called from hub's loop().
  /// @param conn The active connection to send time messages on.
  /// @return Result indicating whether a message was sent and/or the burst completed.
  TimeBurstResult loop(SendspinConnection *conn);

  /// @brief Called when a SERVER_TIME response arrives.
  /// @param conn The connection that received the response.
  /// @param offset Computed time offset from the NTP-style exchange.
  /// @param max_error Half the round-trip delay (RTT proxy).
  /// @param timestamp Client timestamp when measurement was taken.
  /// @return true if this completed the burst (Kalman filter was updated).
  bool on_time_response(SendspinConnection *conn, int64_t offset, int64_t max_error, int64_t timestamp);

  /// @brief Reset state (call on connection loss/change).
  void reset();

 protected:
  static const uint8_t BURST_SIZE = 8;
  static const int64_t BURST_INTERVAL_MS = 10000;
  static const int64_t RESPONSE_TIMEOUT_MS = 10000;

  uint8_t burst_index_{BURST_SIZE};  // starts "complete" so first loop triggers a burst
  int64_t last_burst_complete_time_{0};
  int64_t current_message_sent_time_{0};

  // Flag set by on_time_response() when burst completes, consumed by loop()
  bool pending_burst_completed_{false};

  // Best measurement in current burst
  int64_t best_max_error_{std::numeric_limits<int64_t>::max()};
  int64_t best_offset_{0};
  int64_t best_timestamp_{0};
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
