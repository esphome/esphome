#include "async_stream_response.h"
#if !defined(USE_ESP32) && defined(USE_ARDUINO)

#include <cinttypes>
#include <cstring>

#include "esphome/core/application.h"
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

// Writes the head, then creates the stream with the connection. The library runs _respond() in the network context
// right after the handler returns; clientRelease() then unhooks the request, which frees itself and this response.
// A request that ends before _respond() never creates a stream, so nothing is left to free.
class StreamHandoffResponse final : public AsyncWebServerResponse {
 public:
  StreamHandoffResponse(const char *status, const char *content_type, std::span<const HttpHeader> headers,
                        const char *body, const StreamAcceptor &acceptor)
      : status_(status), content_type_(content_type), headers_(headers), body_(body), acceptor_(acceptor) {}

  void _respond(AsyncWebServerRequest *request) override {
    AsyncClient *client = request->client();
    // The library's default headers only reach its own responses, so this raw head writes them itself
    HttpHeader defaults[WEB_SERVER_DEFAULT_HEADERS_COUNT];
    size_t default_count = 0;
    for (const auto &header : DefaultHeaders::Instance()) {
      if (default_count == WEB_SERVER_DEFAULT_HEADERS_COUNT) {
        break;
      }
      defaults[default_count++] = {header.name().c_str(), header.value().c_str()};
    }
    auto gather = [&](auto &&send) {
      write_stream_head<StreamIovec>(send, this->status_, this->content_type_, this->headers_,
                                     std::span<const HttpHeader>(defaults, default_count), this->body_);
    };
    size_t head_len = 0;
    size_t queued = 0;
    bool short_add = false;
    char scratch[ADD_SCRATCH_SIZE];
    gather([&](StreamIovec *iov, int count) {
      const size_t len = iovecs_total(iov, count);
      head_len += len;
      if (!short_add) {
        const size_t added = add_iovecs(*client, iov, count, scratch, sizeof(scratch));
        queued += added;
        short_add = added != len;
      }
      return true;
    });
    // lwIP can run short of memory while many connections close, so the rest of the head waits for the main loop.
    // The head is text, so the rest is kept as a C string.
    RAMUniquePtr<char[]> head_rest;
    const size_t rest_len = head_len - queued;
    if (rest_len != 0) {
      head_rest = RAMAllocator<char>(RAMAllocator<char>::PREFER_INTERNAL).make_unique_array_for_overwrite(rest_len + 1);
      if (head_rest != nullptr) {
        size_t skip = queued;
        char *out = head_rest.get();
        gather([&](StreamIovec *iov, int count) {
          out += copy_iovecs_after(iov, count, skip, out);
          return true;
        });
        *out = '\0';
      }
    }
    const bool head_kept = rest_len == 0 || head_rest != nullptr;
    AsyncStreamResponse *stream = this->acceptor_.create(this->acceptor_.ctx);
    stream->attach_(request->clientRelease(), std::move(head_rest), head_kept);
    this->acceptor_.publish(this->acceptor_.owner, stream);
  }
  bool _sourceValid() const override { return true; }

 protected:
  const char *status_;
  const char *content_type_;
  std::span<const HttpHeader> headers_;
  const char *body_;
  StreamAcceptor acceptor_;
};

void AsyncStreamResponse::accept(AsyncWebServerRequest *request, const char *status, const char *content_type,
                                 std::span<const HttpHeader> headers, const char *body,
                                 const StreamAcceptor &acceptor) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) the request owns and frees its response
  request->send(new StreamHandoffResponse(status, content_type, headers, body, acceptor));
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
        static_cast<AsyncStreamResponse *>(arg)->disconnected_.store(true, std::memory_order_release);
      },
      this);
  client->onTimeout(
      [](void *arg, AsyncClient *, uint32_t) {
        static_cast<AsyncStreamResponse *>(arg)->ack_timed_out_.store(true, std::memory_order_release);
      },
      this);
  this->client_ = client;
  this->head_rest_ = std::move(head_rest);
  // Without the rest of the head the stream cannot continue. Closing here, inside the client's receive callback,
  // could free the pcb the library still uses after it, so the main loop closes it once the callback has returned.
  this->close_requested_ = !head_kept;
}

bool AsyncStreamResponse::send_head_rest_() {
  char *rest = this->head_rest_.get();
  const size_t len = strlen(rest);
  StreamIovec iov = {rest, len};
  const ssize_t sent = this->send_(&iov, 1);
  if (sent <= 0) {
    return false;
  }
  if (static_cast<size_t>(sent) < len) {
    memmove(rest, rest + sent, len - sent + 1);
    return false;
  }
  this->head_rest_.reset();
  return true;
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
  if (this->head_rest_ != nullptr && !this->send_head_rest_()) {
    return this->close_requested_ ? -1 : 0;
  }
  return this->send_(iov, iovcnt);
}

ssize_t AsyncStreamResponse::send_(StreamIovec *iov, int iovcnt) {
  if (this->close_requested_) {
    return -1;
  }
  // Nothing to send is not a stall
  if (iovecs_total(iov, iovcnt) == 0) {
    return 0;
  }
  if (!this->is_connected()) {
    this->close();
    return -1;
  }
  size_t sent = 0;
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->stall_.is_running() || now - this->last_try_ms_ >= STALL_RETRY_MS) {
    this->last_try_ms_ = now;
    // RP2: the network IRQ may free the pcb between the check and the use, so hold it out across both
    LwIPLock lock;
#ifdef USE_ESP8266
    // Every segment holds a full MSS pbuf on the heap until acked (LWIP_NETIF_TX_SINGLE_PBUF), so write only once
    // the last send is acked, like ESPAsyncWebServer. Not a stall: the library's ACK timeout covers a silent peer.
    if (this->is_connected() && !this->client_->canSend())
      return 0;
#endif
    if (this->is_connected()) {
      // Only an ACK grows the send buffer's free space (lwIP snd_buf). ESPAsyncTCP and RPAsyncTCP call onAck
      // only once everything is acked, so it cannot measure this.
      const size_t space_before = this->client_->space();
      if (space_before > this->space_after_write_)
        this->last_ack_ms_ = now;
      char scratch[ADD_SCRATCH_SIZE];
      sent = add_iovecs(*this->client_, iov, iovcnt, scratch, sizeof(scratch));
      if (sent > 0) {
        const size_t space_after = this->client_->space();
        // On LibreTiny an ACK can arrive while the adds run in the tcpip thread; it leaves more space than they used
        if (space_after + sent > space_before)
          this->last_ack_ms_ = now;
        this->space_after_write_ = space_after;
      }
    }
  }
  if (sent > 0) {
    this->stall_.clear();
    // lwIP keeps queueing into a large buffer while the window is shut, so the buffer may never fill
    if (now - this->last_ack_ms_ >= SEND_STALL_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Closing stuck stream after %" PRIu32 " ms without an ACK", now - this->last_ack_ms_);
      this->abort_();
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
  ESP_LOGW(TAG, "Closing stuck stream after %" PRIu32 " ms %s", this->stall_.elapsed(now), LOG_STR_ARG(reason));
  this->abort_();
  return true;
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
  if (this->close_sent_ || !this->is_connected()) {
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
    this->disconnected_.store(true, std::memory_order_release);
  }
}

void AsyncStreamResponse::loop() {
  if (this->close_requested_) {
    this->close_client_();
    return;
  }
  if (this->ack_timed_out_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "Closing stream after %" PRIu32 " ms without an ACK", SEND_STALL_TIMEOUT_MS);
    this->abort_();
    return;
  }
  if (this->head_rest_ != nullptr) {
    this->send_head_rest_();
  }
}

}  // namespace esphome::web_server_base

#endif  // !USE_ESP32 && USE_ARDUINO
