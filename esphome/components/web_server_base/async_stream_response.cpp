#include "async_stream_response.h"
#if !defined(USE_ESP32) && defined(USE_ARDUINO)

#include <cinttypes>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifndef USE_LIBRETINY
#include <lwip/tcp.h>
#endif

namespace esphome::web_server_base {

ESPHOME_LOG_TAG(TAG, "web_server_base");

static_assert(STREAM_ADD_FLAG_COPY == ASYNC_WRITE_FLAG_COPY && STREAM_ADD_FLAG_MORE == ASYNC_WRITE_FLAG_MORE,
              "add_iovecs() flags must match the TCP library");

// Bytes coalesced into one AsyncClient::add(); on the loop stack, so kept small
static constexpr size_t ADD_SCRATCH_SIZE = 256;

// Closes now and runs the disconnect callback. AsyncTCP (LibreTiny) deprecates close(bool); ESPAsyncTCP and
// RPAsyncTCP need it, as their close() waits for the next poll.
static void close_now(AsyncClient *client) {
#ifdef USE_LIBRETINY
  client->close();
#else
  client->close(true);
#endif
}

#ifndef USE_LIBRETINY
// Reaches the library's protected pcb
struct AsyncClientPcb : AsyncClient {
  static tcp_pcb *of(AsyncClient *client) { return client->*(&AsyncClientPcb::_pcb); }
};
#endif

// Sends a RST and frees the pcb now. A graceful close queues its FIN behind data a stalled reader never takes, so
// it would hold the pcb until the peer leaves. False on ESP8266/RP2, where the caller marks the stream disconnected;
// LibreTiny's callback runs here or later from a queued error event that may still name the client, so never mark it.
static bool abort_now(AsyncClient *client) {
#ifdef USE_LIBRETINY
  // AsyncTCP resets the pcb callbacks in the tcpip thread, aborts, then runs the disconnect callback here
  client->abort();
  return true;
#else
  // ESPAsyncTCP and RPAsyncTCP leave the pcb's error callback set, and RPAsyncTCP's would use the pcb lwIP freed
  tcp_pcb *pcb = AsyncClientPcb::of(client);
  if (pcb != nullptr)
    tcp_err(pcb, nullptr);
  client->abort();
  return false;
#endif
}

// The head strings may sit in flash on ESP8266, where String needs the flash helper to copy them
#ifdef USE_ESP8266
static String head_string(const char *str) { return String(reinterpret_cast<const __FlashStringHelper *>(str)); }
#else
static const char *head_string(const char *str) { return str; }
#endif

// The status line's code, for example 200 for "200 OK"; status may be in flash
static int status_code(const char *status) {
  int code = 0;
  for (int i = 0; i < 3; i++) {
    code = code * 10 + (progmem_read_byte(reinterpret_cast<const uint8_t *>(status + i)) - '0');
  }
  return code;
}

// Queues the head and hands the connection to the stream. The library runs _respond() in the network context right
// after the handler returns; clientRelease() then unhooks the request, which frees itself and this response. A request
// that ends before _respond() frees this response unanswered, which marks the stream disconnected for the main loop.
class StreamHandoffResponse final : public AsyncWebServerResponse {
 public:
  StreamHandoffResponse(AsyncStreamResponse *stream, const char *status, const char *content_type,
                        std::span<const HttpHeader> headers, const char *body)
      : stream_(stream), headers_(headers), body_(body) {
    this->_code = status_code(status);
    this->_contentType = head_string(content_type);
    this->_sendContentLength = false;
  }
  ~StreamHandoffResponse() override {
    if (this->stream_ != nullptr) {
      this->stream_->state_.store(StreamState::STREAM_STATE_DISCONNECTED, std::memory_order_release);
    }
  }

  void _respond(AsyncWebServerRequest *request) override {
    AsyncClient *client = request->client();
    // The library's own head: the status line, its default headers (copied by its constructor) and these
    for (const auto &header : this->headers_) {
      this->addHeader(head_string(header.name), head_string(header.value));
    }
    String head;
    this->_assembleHead(head, request->version());
    if (this->body_ != nullptr) {
      head.concat(head_string(this->body_));
    }
    const size_t added = client->add(head.c_str(), head.length());
    if (added != 0) {
      client->send();
    }
    // lwIP can run short of memory while many connections close: the rest waits for the main loop as a C string
    RAMUniquePtr<char[]> head_rest;
    const size_t rest_len = head.length() - added;
    if (rest_len != 0) {
      head_rest = RAMAllocator<char>(RAMAllocator<char>::PREFER_INTERNAL).make_unique_array_for_overwrite(rest_len + 1);
      if (head_rest != nullptr) {
        std::memcpy(head_rest.get(), head.c_str() + added, rest_len + 1);
      }
    }
    const bool head_kept = rest_len == 0 || head_rest != nullptr;
    // Cleared first: clientRelease() may free the request, and this response with it
    AsyncStreamResponse *stream = this->stream_;
    this->stream_ = nullptr;
    stream->attach_(request->clientRelease(), std::move(head_rest), head_kept);
  }
  bool _sourceValid() const override { return true; }

 protected:
  // Until _respond() hands the connection over
  AsyncStreamResponse *stream_;
  std::span<const HttpHeader> headers_;
  const char *body_;
};

void AsyncStreamResponse::begin(AsyncWebServerRequest *request, const char *status, const char *content_type,
                                std::span<const HttpHeader> headers, const char *body) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) the request owns and frees its response
  request->send(new StreamHandoffResponse(this, status, content_type, headers, body));
}

void AsyncStreamResponse::attach_(AsyncClient *client, RAMUniquePtr<char[]> head_rest, bool head_kept) {
  client->setRxTimeout(0);
  // Backstop for data left unacked once writes stop; each send restarts it
  client->setAckTimeout(SEND_STALL_TIMEOUT_MS);
  client->setNoDelay(true);
  // clientRelease() already cleared the request's data, ack, poll and error handlers
  // Network context: store a flag and nothing else; the main loop frees the client. On LibreTiny async_tcp runs above
  // the loop task and touches nothing after this callback, so the loop cannot free the client under it.
  client->onDisconnect(
      [](void *arg, AsyncClient *) {
        static_cast<AsyncStreamResponse *>(arg)->state_.store(StreamState::STREAM_STATE_DISCONNECTED,
                                                              std::memory_order_release);
      },
      this);
  client->onTimeout(
      [](void *arg, AsyncClient *, uint32_t) {
        static_cast<AsyncStreamResponse *>(arg)->ack_timed_out_.store(true, std::memory_order_release);
      },
      this);
  this->client_ = client;
  // The ACK clock starts with the head
  this->last_ack_ms_ = millis();
  this->head_rest_ = std::move(head_rest);
  // Without the rest of the head the stream cannot continue. Closing here, inside the client's receive callback,
  // could free the pcb the library still uses after it, so the main loop closes it once the callback has returned.
  if (!head_kept) {
    this->close_requested_ = true;
  }
  // Last: the main loop reads everything above once it sees this
  this->state_.store(StreamState::STREAM_STATE_ATTACHED, std::memory_order_release);
}

AsyncStreamResponse::~AsyncStreamResponse() {
  AsyncClient *client = this->client_;
  if (client == nullptr) {
    return;
  }
  // Held so the RP2 network IRQ cannot run while the client goes away
  LwIPLock lock;
  client->onDisconnect(nullptr, nullptr);
  client->onTimeout(nullptr, nullptr);
  delete client;  // NOLINT(cppcoreguidelines-owning-memory)
}

ssize_t AsyncStreamResponse::write(StreamIovec *iov, int iovcnt) {
  if (this->close_requested_) {
    return -1;
  }
  // Gone for good, before or after attaching
  if (!this->is_connected()) {
    this->close();
    return -1;
  }
  // The request has not handed over the connection yet
  if (!this->is_attached()) {
    return 0;
  }
  // The rest of the head goes first; loop() sends it
  if (this->head_rest_ != nullptr) {
    return 0;
  }
  // Nothing to send is not a stall
  if (iovecs_total(iov, iovcnt) == 0) {
    return 0;
  }
  size_t sent = 0;
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->stall_.is_running() || now - this->last_try_ms_ >= STALL_RETRY_MS) {
    this->last_try_ms_ = now;
    // RP2: the network IRQ may free the pcb between the check and the use, so hold it out across both
    LwIPLock lock;
    if (this->is_connected()) {
#ifdef USE_ESP8266
      // Every segment holds a full MSS pbuf on the heap until acked (LWIP_NETIF_TX_SINGLE_PBUF), so write only once
      // the last send is acked, like ESPAsyncWebServer. canSend() is also false when a failed send never armed the
      // library's ACK timeout, so the ACK age is checked here too.
      if (!this->client_->canSend()) {
        if (now - this->last_ack_ms_ >= SEND_STALL_TIMEOUT_MS) {
          this->close_stuck_(now - this->last_ack_ms_, LOG_STR("without an ACK"));
          return -1;
        }
        return 0;
      }
#endif
      // Only an ACK grows the send buffer's free space (lwIP snd_buf). ESPAsyncTCP and RPAsyncTCP call onAck
      // only once everything is acked, so it cannot measure this.
      const size_t space_before = this->client_->space();
      if (space_before > this->space_after_write_)
        this->last_ack_ms_ = now;
      char scratch[ADD_SCRATCH_SIZE];
      sent = add_iovecs(*this->client_, iov, iovcnt, scratch, sizeof(scratch));
      if (sent > 0) {
        const size_t space_after = this->client_->space();
        // On LibreTiny an ACK can arrive while the adds run in the tcpip thread; it leaves more space than they used.
        // The space is then recorded as if it had not, so the ACKs for these bytes still count at the next write: a
        // buffer the ACK emptied would otherwise look unchanged after an idle pause and the stream would be closed.
        if (space_after + sent > space_before) {
          this->last_ack_ms_ = now;
          // add_iovecs() adds in several calls, so with ACKs between them it can queue more than space_before
          this->space_after_write_ = sent < space_before ? space_before - sent : 0;
        } else {
          this->space_after_write_ = space_after;
        }
      }
    }
  }
  if (sent > 0) {
    this->stall_.clear();
    // lwIP keeps queueing into a large buffer while the window is shut, so the buffer may never fill
    if (now - this->last_ack_ms_ >= SEND_STALL_TIMEOUT_MS) {
      this->close_stuck_(now - this->last_ack_ms_, LOG_STR("without an ACK"));
      return -1;
    }
    return static_cast<ssize_t>(sent);
  }
  // Send buffer full, try again later; a buffer that stays full past the stall timeout is closed
  return this->stall_or_close_(LOG_STR("without send progress")) ? -1 : 0;
}

ssize_t AsyncStreamResponse::write(const void *data, size_t len) {
  StreamIovec iov = {const_cast<void *>(data), len};
  return this->write(&iov, 1);
}

bool AsyncStreamResponse::stall_or_close_(const LogString *reason) {
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->stall_.start_or_expired(now, SEND_STALL_TIMEOUT_MS)) {
    return false;
  }
  this->close_stuck_(this->stall_.elapsed(now), reason);
  return true;
}

void AsyncStreamResponse::close_stuck_(uint32_t elapsed_ms, const LogString *reason) {
  ESP_LOGW(TAG, "Closing stuck stream after %" PRIu32 " ms %s", elapsed_ms, LOG_STR_ARG(reason));
  this->abort_();
}

void AsyncStreamResponse::abort_() {
  this->close_requested_ = true;
  this->close_client_(true);
}

void AsyncStreamResponse::close() {
  this->close_requested_ = true;
  this->close_client_();
}

void AsyncStreamResponse::close_client_(bool abort) {
  // Before the connection is ours, close_requested_ keeps the close and loop() sends it once attached
  if (this->close_sent_ || !this->is_connected() || !this->is_attached()) {
    return;
  }
  this->close_sent_ = true;
  LwIPLock lock;
  if (!this->is_connected()) {
    return;
  }
  if (!abort) {
    close_now(this->client_);
  } else if (!abort_now(this->client_)) {
    this->state_.store(StreamState::STREAM_STATE_DISCONNECTED, std::memory_order_release);
  }
}

void AsyncStreamResponse::loop() {
  if (this->close_requested_) {
    this->close_client_();
    return;
  }
  if (this->ack_timed_out_.load(std::memory_order_acquire)) {
    this->close_stuck_(SEND_STALL_TIMEOUT_MS, LOG_STR("without an ACK"));
    return;
  }
  if (this->head_rest_ != nullptr) {
    // Moved out so write() takes it; the stall and ACK clocks cover it like any write
    RAMUniquePtr<char[]> rest = std::move(this->head_rest_);
    const size_t len = strlen(rest.get());
    const ssize_t sent = this->write(rest.get(), len);
    if (sent >= 0 && static_cast<size_t>(sent) < len) {
      memmove(rest.get(), rest.get() + sent, len - sent + 1);
      this->head_rest_ = std::move(rest);
    }
  }
}

}  // namespace esphome::web_server_base

#endif  // !USE_ESP32 && USE_ARDUINO
