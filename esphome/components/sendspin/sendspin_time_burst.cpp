#include "sendspin_time_burst.h"

#ifdef USE_ESP32

#include "sendspin_connection.h"
#include "sendspin_time_filter.h"

#include "esphome/core/log.h"

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.time_burst";

TimeBurstResult SendspinTimeBurst::loop(SendspinConnection *conn) {
  // Consume burst completion flag set by on_time_response() (called between loop() invocations)
  bool burst_completed_by_response = this->pending_burst_completed_;
  this->pending_burst_completed_ = false;

  if (conn == nullptr || !conn->is_connected() || !conn->is_handshake_complete()) {
    return {.sent = false, .burst_completed = burst_completed_by_response};
  }

  const int64_t now_us = esp_timer_get_time();
  const int64_t now_ms = now_us / 1000LL;

  // State 1: Burst complete / inter-burst wait
  if (this->burst_index_ >= BURST_SIZE) {
    if (now_ms - this->last_burst_complete_time_ < BURST_INTERVAL_MS) {
      return {.sent = false, .burst_completed = burst_completed_by_response};
    }
    // Start a new burst
    this->burst_index_ = 0;
    this->best_max_error_ = std::numeric_limits<int64_t>::max();
    ESP_LOGV(TAG, "Starting new time burst");
    // Fall through to send first message
  }

  // State 2: Waiting for response — check timeout
  if (conn->is_pending_time_message()) {
    if (now_ms - this->current_message_sent_time_ > RESPONSE_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Time message %u/%u timed out", this->burst_index_ + 1, BURST_SIZE);
      conn->set_pending_time_message(false);
      this->burst_index_++;

      // If burst now complete, apply best measurement
      if (this->burst_index_ >= BURST_SIZE) {
        auto *time_filter = conn->get_time_filter();
        if (time_filter != nullptr && this->best_max_error_ < std::numeric_limits<int64_t>::max()) {
          time_filter->update(this->best_offset_, this->best_max_error_, this->best_timestamp_);
          ESP_LOGV(TAG, "Burst complete (with timeouts), best max_error: %" PRId64 " us", this->best_max_error_);
        }
        this->last_burst_complete_time_ = now_ms;
        return {.sent = false, .burst_completed = true};
      }
    }
    return {.sent = false, .burst_completed = burst_completed_by_response};
  }

  // State 3: Ready to send next message in burst
  // The time replacement (transmitted_time + actual_transmit_time) is pushed to the
  // connection's thread-safe queue internally by send_time_message's send callback.
  bool queued = conn->send_time_message(nullptr);

  if (queued) {
    conn->set_pending_time_message(true);
    this->current_message_sent_time_ = now_ms;
    ESP_LOGV(TAG, "Sent time message %u/%u", this->burst_index_ + 1, BURST_SIZE);
    return {.sent = true, .burst_completed = burst_completed_by_response};
  }

  return {.sent = false, .burst_completed = burst_completed_by_response};
}

bool SendspinTimeBurst::on_time_response(SendspinConnection *conn, int64_t offset, int64_t max_error,
                                         int64_t timestamp) {
  // Track the best (lowest RTT) measurement in this burst
  if (max_error < this->best_max_error_) {
    this->best_max_error_ = max_error;
    this->best_offset_ = offset;
    this->best_timestamp_ = timestamp;
  }

  conn->set_pending_time_message(false);
  this->burst_index_++;

  // Check if burst is complete
  if (this->burst_index_ >= BURST_SIZE) {
    auto *time_filter = conn->get_time_filter();
    if (time_filter != nullptr) {
      time_filter->update(this->best_offset_, this->best_max_error_, this->best_timestamp_);
      ESP_LOGV(TAG, "Burst complete, best max_error: %" PRId64 " us", this->best_max_error_);
    }
    this->last_burst_complete_time_ = esp_timer_get_time() / 1000LL;
    this->pending_burst_completed_ = true;
    return true;
  }

  return false;
}

void SendspinTimeBurst::reset() {
  this->burst_index_ = BURST_SIZE;  // "complete" state — next loop will wait for interval
  this->last_burst_complete_time_ = 0;
  this->current_message_sent_time_ = 0;
  this->pending_burst_completed_ = false;
  this->best_max_error_ = std::numeric_limits<int64_t>::max();
  this->best_offset_ = 0;
  this->best_timestamp_ = 0;
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
