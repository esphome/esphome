#pragma once

#include "esphome/core/defines.h"
#if !defined(USE_ESP32) && defined(USE_ARDUINO)

#include <ESPAsyncWebServer.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <sys/types.h>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "stream_head.h"

namespace esphome::web_server_base {

/// One entry of the gather list AsyncStreamResponse::write() takes; the TCP libraries take plain buffers
struct StreamIovec {
  void *iov_base;
  size_t iov_len;
};

/// A long lived response the main loop writes to without blocking, on ESPAsyncWebServer; the same interface as
/// web_server_idf's AsyncStreamResponse. The network callbacks only set flags, and the main loop owns and frees the
/// AsyncClient.
enum class StreamState : uint8_t {
  STREAM_STATE_PENDING,       // begin() ran; the request has not handed over the connection yet
  STREAM_STATE_ATTACHED,      // the connection is ours and the head is queued
  STREAM_STATE_DISCONNECTED,  // gone; no network callback references the stream any more
};

class AsyncStreamResponse {
 public:
  AsyncStreamResponse() = default;
  AsyncStreamResponse(const AsyncStreamResponse &) = delete;
  AsyncStreamResponse &operator=(const AsyncStreamResponse &) = delete;
  /// Main loop, once safe_to_delete(): frees the connection
  ~AsyncStreamResponse();

  /// Network context, from a request handler, once. Queues the library's head for status (for example "200 OK":
  /// status line, default headers, Content-Type) with headers and body (a C string or null; all may be in flash),
  /// then takes the connection from the request. A chunked response passes {"Transfer-Encoding", "chunked"} in
  /// headers and frames each chunk itself. The head is built after the handler returns, so headers, their strings
  /// and body must outlive the request (static data in practice); the ESP-IDF begin() sends it at once.
  void begin(AsyncWebServerRequest *request, const char *status, const char *content_type,
             std::span<const HttpHeader> headers, const char *body = nullptr);

  /// Main loop. Non-blocking gather write. Returns the bytes accepted, which may be fewer than offered: the caller
  /// keeps the rest and continues from there. 0 means the send buffer is full, the last send is not acked yet (ESP8266
  /// only) or the connection is not ours yet; -1 means the stream is closing. SEND_STALL_TIMEOUT_MS of refused writes,
  /// or of writes with no ACK in between, also aborts it (RST).
  ssize_t write(StreamIovec *iov, int iovcnt);
  ssize_t write(const void *data, size_t len);
  /// Main loop. Closes the connection; safe to call more than once.
  void close();
  /// Main loop, every pass. Aborts (RST) a stream whose last send went unacknowledged for SEND_STALL_TIMEOUT_MS.
  void loop();

  /// The connection is taken from the request and the head queued; write() needs it.
  bool is_attached() const {
    return this->state_.load(std::memory_order_acquire) == StreamState::STREAM_STATE_ATTACHED;
  }
  /// The client is still connected.
  bool is_connected() const {
    return this->state_.load(std::memory_order_acquire) != StreamState::STREAM_STATE_DISCONNECTED;
  }
  bool is_closing() const { return this->close_requested_; }
  /// An id for log lines only
  int log_id() const { return static_cast<int>(reinterpret_cast<uintptr_t>(this) & 0xFFFF); }
  /// The connection is gone; no network callback still references this object.
  bool safe_to_delete() const {
    return this->state_.load(std::memory_order_acquire) == StreamState::STREAM_STATE_DISCONNECTED;
  }

  static constexpr uint32_t SEND_STALL_TIMEOUT_MS = 20000;
  // While stalled, retry at most this often: on RP2 each try releases the lwIP lock, which postpones a polled
  // Ethernet chip's 20 ms receive poll
  static constexpr uint32_t STALL_RETRY_MS = 100;

 protected:
  friend class StreamHandoffResponse;

  // Network context: take the connection and the part of the head not queued yet, as a C string; head_kept false
  // (no memory for that part) closes it
  void attach_(AsyncClient *client, RAMUniquePtr<char[]> head_rest, bool head_kept);
  // Starts the stall clock; once it has run SEND_STALL_TIMEOUT_MS, logs reason and aborts. True when it did.
  bool stall_or_close_(const LogString *reason);
  // Graceful FIN, or with abort a RST that frees the pcb now (a stuck stream's FIN would queue behind its data)
  void close_client_(bool abort = false);
  void abort_();
  // Logs how long the stream made no progress and why, then aborts it
  void __attribute__((cold)) close_stuck_(uint32_t elapsed_ms, const LogString *reason);

  // Set before the state turns attached; the main loop owns it and frees it once disconnected
  AsyncClient *client_{nullptr};
  // The part of the head the send buffer could not take in the network context; sent before anything else
  RAMUniquePtr<char[]> head_rest_;
  // Runs while sends make no progress
  StallClock stall_;
  uint32_t last_try_ms_{0};
  // Send buffer free after the last write that queued bytes, and when an ACK was last seen (from attach_() on);
  // main loop only once attached
  size_t space_after_write_{0};
  uint32_t last_ack_ms_{0};
  // Set from the network callbacks with plain stores (no read-modify-write: RP2040 has none)
  // Only moves forward (pending, attached, disconnected), set with plain stores from the network context, the
  // request's teardown and abort_now(); one byte keeps the three main loop flags below in the same word
  std::atomic<StreamState> state_{StreamState::STREAM_STATE_PENDING};
  std::atomic<bool> ack_timed_out_{false};
  // Main loop only, apart from attach_() setting close_requested_ when the head was lost
  bool close_requested_{false};
  bool close_sent_{false};
};

}  // namespace esphome::web_server_base

#endif  // !USE_ESP32 && USE_ARDUINO
