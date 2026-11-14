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

class AsyncStreamResponse;

/// Who makes and takes the stream accept() creates: create(ctx) allocates it, publish(owner, stream) hands the
/// attached stream to the main loop. Both run in the network context.
struct StreamAcceptor {
  AsyncStreamResponse *(*create)(void *ctx);
  void (*publish)(void *owner, AsyncStreamResponse *stream);
  void *owner;
  void *ctx;
};

/// A long lived response the main loop writes to without blocking, on ESPAsyncWebServer. It exists only once the
/// head is queued and the connection taken from the request; the network callbacks only set flags, and the main
/// loop owns and frees the AsyncClient. Writes like web_server_idf's AsyncStreamResponse.
class AsyncStreamResponse {
 public:
  AsyncStreamResponse() = default;
  AsyncStreamResponse(const AsyncStreamResponse &) = delete;
  AsyncStreamResponse &operator=(const AsyncStreamResponse &) = delete;
  /// Main loop, once safe_to_delete(): frees the connection
  ~AsyncStreamResponse();

  /// Network context, from a request handler. Queues the status line, Content-Type, headers, the default headers and
  /// body (a C string or null), then has the acceptor create the stream, attach the connection and publish it. A
  /// chunked response passes {"Transfer-Encoding", "chunked"} as its first header and frames each chunk itself.
  static void accept(AsyncWebServerRequest *request, const char *status, const char *content_type,
                     std::span<const HttpHeader> headers, const char *body, const StreamAcceptor &acceptor);

  /// Main loop. Non-blocking gather write. Returns the bytes accepted, which may be fewer than offered: the caller
  /// keeps the rest and continues from there. 0 means the send buffer is full or the connection is not ours yet; -1
  /// means the stream is closing. SEND_STALL_TIMEOUT_MS of refused writes, or of writes with no ACK in between,
  /// also aborts it (RST).
  ssize_t write(StreamIovec *iov, int iovcnt);
  ssize_t write(const void *data, size_t len);
  /// Main loop. Closes the connection; safe to call more than once.
  void close();
  /// Main loop, every pass. Aborts (RST) a stream whose last send went unacknowledged for SEND_STALL_TIMEOUT_MS.
  void loop();

  /// Always true: a stream is published only once it has its connection.
  bool is_attached() const { return true; }
  /// The client is still connected.
  bool is_connected() const { return !this->disconnected_.load(std::memory_order_acquire); }
  bool is_closing() const { return this->close_requested_; }
  /// An id for log lines only
  int log_id() const { return static_cast<int>(reinterpret_cast<uintptr_t>(this) & 0xFFFF); }
  /// The connection is gone; no network callback still references this object.
  bool safe_to_delete() const { return this->disconnected_.load(std::memory_order_acquire); }

  static constexpr uint32_t SEND_STALL_TIMEOUT_MS = 20000;
  // While stalled, retry at most this often: on RP2 each try releases the lwIP lock, which postpones a polled
  // Ethernet chip's 20 ms receive poll
  static constexpr uint32_t STALL_RETRY_MS = 100;

 protected:
  friend class StreamHandoffResponse;

  // Network context, before publishing: take the connection; a head not queued whole closes it
  void attach_(AsyncClient *client, bool head_sent);
  // Starts the stall clock; once it has run SEND_STALL_TIMEOUT_MS, logs reason and aborts. True when it did.
  bool stall_or_close_(const LogString *reason);
  // Graceful FIN, or with abort a RST that frees the pcb now (a stuck stream's FIN would queue behind its data)
  void close_client_(bool abort = false);
  void abort_();

  // Set before publishing; the main loop owns it and frees it once disconnected_
  AsyncClient *client_{nullptr};
  // Runs while sends make no progress
  StallClock stall_;
  uint32_t last_try_ms_{0};
  // Send buffer free after the last write that queued bytes, and when an ACK was last seen; main loop only
  size_t space_after_write_{0};
  uint32_t last_ack_ms_{0};
  // Set from the network callbacks with plain stores (no read-modify-write: RP2040 has none)
  std::atomic<bool> disconnected_{false};
  std::atomic<bool> ack_timed_out_{false};
  // Main loop only
  bool close_requested_{false};
  bool close_sent_{false};
};

}  // namespace esphome::web_server_base

#endif  // !USE_ESP32 && USE_ARDUINO
