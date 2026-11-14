#pragma once

#include "esphome/core/defines.h"
#if !defined(USE_ESP32) && defined(USE_ARDUINO)

#include <ESPAsyncWebServer.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sys/types.h>

#include "esphome/core/log.h"
#include "stream_head.h"

namespace esphome::web_server_base {

/// One entry of the gather list AsyncStreamResponse::write() takes; the TCP libraries take plain buffers
struct StreamIovec {
  void *iov_base;
  size_t iov_len;
};

/// A long lived response the main loop writes to without blocking, on ESPAsyncWebServer. The connection is taken
/// from the request once the head is queued; the network callbacks only set flags, and the main loop owns and frees
/// the AsyncClient. Same API as web_server_idf's AsyncStreamResponse.
class AsyncStreamResponse {
 public:
  AsyncStreamResponse() = default;
  AsyncStreamResponse(const AsyncStreamResponse &) = delete;
  AsyncStreamResponse &operator=(const AsyncStreamResponse &) = delete;
  /// Main loop, once safe_to_delete(): frees the connection
  ~AsyncStreamResponse();

  /// Network context, once. Queues the status line, Content-Type, headers, the default headers and body (a C string or
  /// null) and takes the connection from the request. A chunked response passes {"Transfer-Encoding", "chunked"} as
  /// its first header and frames each chunk itself.
  void begin(AsyncWebServerRequest *request, const char *status, const char *content_type,
             std::span<const HttpHeader> headers, const char *body = nullptr);

  /// Main loop. Non-blocking gather write. Returns the bytes accepted, which may be fewer than offered: the caller
  /// keeps the rest and continues from there. 0 means the send buffer is full; -1 means the stream is closing.
  /// Refused writes for SEND_STALL_TIMEOUT_MS in a row also close it.
  ssize_t write(StreamIovec *iov, int iovcnt);
  ssize_t write(const void *data, size_t len);
  /// Main loop. Closes the connection; safe to call more than once.
  void close();
  /// Main loop, every pass. Closes a stream whose last send went unacknowledged for SEND_STALL_TIMEOUT_MS.
  void loop();

  /// The connection has been taken from the request and the head is queued.
  bool is_attached() const { return this->attached_.load(std::memory_order_acquire); }
  /// The client is still connected.
  bool is_connected() const { return !this->disconnected_.load(std::memory_order_acquire); }
  bool is_closing() const { return this->close_requested_; }
  /// An id for log lines only
  int log_id() const { return static_cast<int>(reinterpret_cast<uintptr_t>(this) & 0xFFFF); }
  /// Sends are making no progress; after write() returns -1 this tells a send timeout from a send error.
  bool is_stalled() const { return this->stall_.is_running(); }
  /// The connection is gone; no network callback still references this object.
  bool safe_to_delete() const { return this->disconnected_.load(std::memory_order_acquire); }

  static constexpr uint32_t SEND_STALL_TIMEOUT_MS = 20000;

 protected:
  friend class StreamHandoffResponse;

  // Network context: take the connection once the head is queued
  void attach_(AsyncClient *client);
  // Starts the stall clock; once it has run SEND_STALL_TIMEOUT_MS, logs reason and closes. True when it closed.
  bool stall_or_close_(const LogString *reason);
  void close_client_();

  // Main loop owns it; freed once disconnected_
  AsyncClient *client_{nullptr};
  // Runs while sends make no progress; a running clock keeps its start time
  StallClock stall_;
  // Set from the network callbacks with plain stores (no read-modify-write: RP2040 has none)
  std::atomic<bool> attached_{false};
  std::atomic<bool> disconnected_{false};
  std::atomic<bool> ack_timed_out_{false};
  // Main loop only
  bool close_requested_{false};
  bool close_sent_{false};
};

}  // namespace esphome::web_server_base

#endif  // !USE_ESP32 && USE_ARDUINO
