#include "sendspin_connection.h"

#ifdef USE_ESP32

#include "esphome/components/json/json_util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.connection";

SendspinConnection::~SendspinConnection() {
  this->deallocate_websocket_payload_();
  if (this->time_replacement_queue_ != nullptr) {
    vQueueDelete(this->time_replacement_queue_);
    this->time_replacement_queue_ = nullptr;
  }
}

void SendspinConnection::deallocate_websocket_payload_() {
  if (this->websocket_payload_ != nullptr) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
    allocator.deallocate(this->websocket_payload_, this->websocket_len_);
    this->websocket_payload_ = nullptr;
  }
  this->websocket_write_offset_ = 0;
  this->websocket_len_ = 0;
}

void SendspinConnection::reset_websocket_payload_() { this->websocket_write_offset_ = 0; }

void SendspinConnection::init_time_filter() {
  this->time_filter_ = make_unique<SendspinTimeFilter>(
      TIME_FILTER_PROCESS_STD_DEV, TIME_FILTER_DRIFT_PROCESS_STD_DEV, TIME_FILTER_FORGET_FACTOR,
      TIME_FILTER_ADAPTIVE_CUTOFF, TIME_FILTER_MIN_SAMPLES, TIME_FILTER_DRIFT_SIGNIFICANCE_THRESHOLD);
  if (this->time_replacement_queue_ == nullptr) {
    this->time_replacement_queue_ = xQueueCreate(1, sizeof(TimeTransmittedReplacement));
  }
}

TimeTransmittedReplacement SendspinConnection::peek_time_replacement() const {
  TimeTransmittedReplacement replacement{};
  if (this->time_replacement_queue_ != nullptr) {
    xQueuePeek(this->time_replacement_queue_, &replacement, 0);
  }
  return replacement;
}

bool SendspinConnection::send_time_message(SendCompleteCallback cb) {
  int64_t now = esp_timer_get_time();

  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  std::string serialized_text = json::build_json([now](JsonObject root) {
    root["type"] = "client/time";
    root["payload"]["client_transmitted"] = now;
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

  // Wrap the caller's callback to push the time replacement into the queue
  // after the message is actually sent. This is thread-safe: the queue handles
  // synchronization between the send thread and the receive thread.
  QueueHandle_t queue = this->time_replacement_queue_;
  auto wrapped_cb = [now, queue, cb = std::move(cb)](bool success, int64_t actual_send_time) {
    if (success && queue != nullptr) {
      TimeTransmittedReplacement replacement{now, actual_send_time};
      xQueueOverwrite(queue, &replacement);
    }
    if (cb) {
      cb(success, actual_send_time);
    }
  };

  return this->send_text_message(serialized_text, std::move(wrapped_cb)) == ESP_OK;
}

esp_err_t SendspinConnection::send_goodbye_reason(SendspinGoodbyeReason reason, SendCompleteCallback on_complete) {
  return this->send_text_message(format_client_goodbye_message(reason), std::move(on_complete));
}

uint8_t *SendspinConnection::prepare_receive_buffer_(size_t data_len) {
  auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);

  if (this->websocket_payload_ == nullptr) {
    // First fragment - allocate new buffer
    this->websocket_payload_ = allocator.allocate(data_len);
    if (this->websocket_payload_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate %zu bytes for websocket payload", data_len);
      return nullptr;
    }
    this->websocket_len_ = data_len;
    this->websocket_write_offset_ = 0;
  } else if (this->websocket_write_offset_ + data_len > this->websocket_len_) {
    // Need to expand buffer for additional fragment
    size_t new_len = this->websocket_write_offset_ + data_len;
    uint8_t *new_payload = allocator.reallocate(this->websocket_payload_, new_len);
    if (new_payload == nullptr) {
      ESP_LOGE(TAG, "Failed to expand websocket payload to %zu bytes", new_len);
      this->deallocate_websocket_payload_();
      return nullptr;
    }
    this->websocket_payload_ = new_payload;
    this->websocket_len_ = new_len;
  }

  return this->websocket_payload_ + this->websocket_write_offset_;
}

void SendspinConnection::commit_receive_buffer_(size_t data_len) { this->websocket_write_offset_ += data_len; }

void SendspinConnection::dispatch_completed_message_(bool is_text, int64_t receive_time) {
  if (this->websocket_payload_ == nullptr) {
    return;
  }

  if (is_text) {
    // Create string from payload for JSON processing
    const std::string message(this->websocket_payload_, this->websocket_payload_ + this->websocket_write_offset_);

    // Invoke JSON message callback
    if (this->on_json_message) {
      this->on_json_message(this, message, receive_time);
    }
  } else {
    // Binary message - connection retains buffer ownership, callback reads in-place
    if (this->on_binary_message) {
      this->on_binary_message(this, this->websocket_payload_, this->websocket_write_offset_);
    }
  }

  // Reset write offset for next message; keep buffer allocated for reuse
  this->reset_websocket_payload_();
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
