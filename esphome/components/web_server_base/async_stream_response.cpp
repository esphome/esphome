#include "async_stream_response.h"
#if !defined(USE_ESP32) && defined(USE_ARDUINO)

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::web_server_base {

ESPHOME_LOG_TAG(TAG, "web_server_base");

static_assert(STREAM_ADD_FLAG_COPY == ASYNC_WRITE_FLAG_COPY && STREAM_ADD_FLAG_MORE == ASYNC_WRITE_FLAG_MORE,
              "add_iovecs() flags must match the TCP library");

// Bytes coalesced into one AsyncClient::add(); on the loop stack, so kept small
static constexpr size_t ADD_SCRATCH_SIZE = 256;

// Closes now and runs the disconnect callback. AsyncTCP (LibreTiny) deprecates close(bool); ESPAsyncTCP and
// RPAsyncTCP need it, as their close() waits for the next poll. Never abort(): it skips the disconnect callback.
static void close_now(AsyncClient *client) {
#ifdef USE_LIBRETINY
  client->close();
#else
  client->close(true);
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
    char scratch[ADD_SCRATCH_SIZE];
    const bool head_sent = write_stream_head<StreamIovec>(
        [client, &scratch](StreamIovec *iov, int count) {
          return add_iovecs(*client, iov, count, scratch, sizeof(scratch)) == iovecs_total(iov, count);
        },
        this->status_, this->content_type_, this->headers_, std::span<const HttpHeader>(defaults, default_count),
        this->body_);
    AsyncStreamResponse *stream = this->acceptor_.create(this->acceptor_.ctx);
    stream->attach_(request->clientRelease(), head_sent);
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

void AsyncStreamResponse::attach_(AsyncClient *client, bool head_sent) {
  client->setRxTimeout(0);
  // Covers data left unacked with nothing more to send (each send restarts it, which write() covers)
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
  // A partly queued head must not be continued by later writes. Closing here, inside the client's receive callback,
  // could free the pcb the library still uses after it, so the main loop closes it once the callback has returned.
  this->close_requested_ = !head_sent;
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
  // Nothing to send is not a stall
  if (iovecs_total(iov, iovcnt) == 0) {
    return 0;
  }
  if (!this->is_connected()) {
    this->close();
    return -1;
  }
  size_t sent = 0;
  bool acked = true;
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->stall_.is_running() || now - this->last_try_ms_ >= STALL_RETRY_MS) {
    this->last_try_ms_ = now;
    // RP2: the network IRQ may free the pcb between the check and the use, so hold it out across both
    LwIPLock lock;
    if (this->is_connected()) {
      // An ACK since the last write frees send buffer; with nothing outstanding the buffer is at its largest.
      // ESPAsyncTCP and RPAsyncTCP call onAck only once everything is acked, so it cannot measure this.
      const size_t space = this->client_->space();
      acked = space > this->space_after_write_ || space >= this->max_space_;
      if (space > this->max_space_)
        this->max_space_ = space;
      char scratch[ADD_SCRATCH_SIZE];
      sent = add_iovecs(*this->client_, iov, iovcnt, scratch, sizeof(scratch));
      this->space_after_write_ = this->client_->space();
    }
  }
  if (sent > 0 && acked) {
    this->stall_.clear();
    return static_cast<ssize_t>(sent);
  }
  if (sent > 0) {
    // Queued, but nothing acked since the last write: a reader whose window stays shut fills a large send buffer
    // only slowly, so this counts as no progress too
    return this->stall_or_close_(LOG_STR("without an ACK")) ? -1 : static_cast<ssize_t>(sent);
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
  this->close();
  return true;
}

void AsyncStreamResponse::close() {
  this->close_requested_ = true;
  this->close_client_();
}

void AsyncStreamResponse::close_client_() {
  if (this->close_sent_ || !this->is_connected()) {
    return;
  }
  this->close_sent_ = true;
  LwIPLock lock;
  if (this->is_connected()) {
    close_now(this->client_);
  }
}

void AsyncStreamResponse::loop() {
  if (this->close_requested_) {
    this->close_client_();
    return;
  }
  if (this->ack_timed_out_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "Closing stream after %" PRIu32 " ms without an ACK", SEND_STALL_TIMEOUT_MS);
    this->close();
  }
}

}  // namespace esphome::web_server_base

#endif  // !USE_ESP32 && USE_ARDUINO
