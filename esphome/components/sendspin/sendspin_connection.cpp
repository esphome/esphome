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

#include "sendspin_connection.h"

#ifdef USE_ESP_IDF

#include "esphome/components/json/json_util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.connection";

SendspinConnection::~SendspinConnection() { this->deallocate_websocket_payload_(); }

void SendspinConnection::deallocate_websocket_payload_() {
  if (this->websocket_payload_ != nullptr) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
    allocator.deallocate(this->websocket_payload_, this->websocket_len_);
    this->websocket_payload_ = nullptr;
  }
  this->websocket_write_offset_ = 0;
  this->websocket_len_ = 0;
}

void SendspinConnection::init_time_filter(double process_error, double forget_factor) {
  this->kalman_process_error_ = process_error;
  this->kalman_forget_factor_ = forget_factor;

  // Create time filter with drift process variance set to 0.0 (no drift tracking)
  this->time_filter_ = make_unique<SendspinTimeFilter>(process_error, 0.0, forget_factor);
}

bool SendspinConnection::send_time_message(SendCompleteCallback cb) {
  int64_t now = esp_timer_get_time();

  this->last_time_message_.transmitted_time = now;
  this->last_time_message_.actual_transmit_time = now;  // will be overwritten by callback

  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  std::string serialized_text = json::build_json([now](JsonObject root) {
    root["type"] = "client/time";
    root["payload"]["client_transmitted"] = now;
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
  return this->send_text_message(serialized_text, std::move(cb)) == ESP_OK;
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

    // Always deallocate after JSON processing
    this->deallocate_websocket_payload_();
  } else {
    // Binary message
    if (this->on_binary_message) {
      // Ask callback if it wants to take ownership of the payload
      bool should_deallocate = this->on_binary_message(this, this->websocket_payload_, this->websocket_write_offset_);
      if (should_deallocate) {
        // Callback did not take ownership, we must deallocate
        this->deallocate_websocket_payload_();
      } else {
        // Ownership transferred, just clear the pointer and reset lengths
        this->websocket_payload_ = nullptr;
        this->websocket_write_offset_ = 0;
        this->websocket_len_ = 0;
      }
    } else {
      // No callback registered, deallocate
      this->deallocate_websocket_payload_();
    }
  }
}

}  // namespace sendspin
}  // namespace esphome

#endif
