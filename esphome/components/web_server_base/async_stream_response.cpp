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

// Hands the connection to the stream once the head is queued. The library runs _respond() in the network context
// right after the handler returns; clientRelease() then unhooks the request, which frees itself and this response.
class StreamHandoffResponse final : public AsyncWebServerResponse {
 public:
  StreamHandoffResponse(AsyncStreamResponse *stream, const char *status, const char *content_type,
                        std::span<const HttpHeader> headers, const char *body)
      : stream_(stream), status_(status), content_type_(content_type), headers_(headers), body_(body) {}
  // A request that ends before _respond() never hands over; the stream must still be freed
  ~StreamHandoffResponse() override {
    if (this->stream_ != nullptr) {
      this->stream_->disconnected_.store(true, std::memory_order_release);
    }
  }

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
          size_t total = 0;
          for (int i = 0; i < count; i++) {
            total += iov[i].iov_len;
          }
          return add_iovecs(*client, iov, count, scratch, sizeof(scratch)) == total;
        },
        this->status_, this->content_type_, this->headers_, std::span<const HttpHeader>(defaults, default_count),
        this->body_);
    AsyncStreamResponse *stream = this->stream_;
    this->stream_ = nullptr;
    stream->attach_(request->clientRelease());
    if (!head_sent) {
      // A partly queued head must not be continued by later writes; the disconnect frees the stream
      stream->client_->close(true);
    }
  }
  bool _sourceValid() const override { return true; }

 protected:
  AsyncStreamResponse *stream_;
  const char *status_;
  const char *content_type_;
  std::span<const HttpHeader> headers_;
  const char *body_;
};

void AsyncStreamResponse::begin(AsyncWebServerRequest *request, const char *status, const char *content_type,
                                std::span<const HttpHeader> headers, const char *body) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) the request owns and frees its response
  request->send(new StreamHandoffResponse(this, status, content_type, headers, body));
}

void AsyncStreamResponse::attach_(AsyncClient *client) {
  this->client_ = client;
  client->setRxTimeout(0);
  // A send with no ACK for this long closes the stream, the same timeout as a full send buffer
  client->setAckTimeout(SEND_STALL_TIMEOUT_MS);
  client->setNoDelay(true);
  // clientRelease() already cleared the request's data, ack, poll and error handlers
  // Network context: store a flag and nothing else; the main loop frees the client
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
  this->attached_.store(true, std::memory_order_release);
}

AsyncStreamResponse::~AsyncStreamResponse() {
  if (this->client_ == nullptr) {
    return;
  }
  // Held so the RP2 network IRQ cannot run while the client goes away
  LwIPLock lock;
  this->client_->onDisconnect(nullptr, nullptr);
  this->client_->onTimeout(nullptr, nullptr);
  delete this->client_;  // NOLINT(cppcoreguidelines-owning-memory)
}

ssize_t AsyncStreamResponse::write(StreamIovec *iov, int iovcnt) {
  if (this->close_requested_) {
    return -1;
  }
  // Nothing to send is not a stall
  size_t total = 0;
  for (int i = 0; i < iovcnt; i++) {
    total += iov[i].iov_len;
  }
  if (total == 0) {
    return 0;
  }
  if (!this->is_connected()) {
    this->close();
    return -1;
  }
  size_t sent = 0;
  if (this->is_attached()) {
    // RP2: the network IRQ may free the pcb between the check and the use, so hold it out across both
    LwIPLock lock;
    if (this->is_connected()) {
      char scratch[ADD_SCRATCH_SIZE];
      sent = add_iovecs(*this->client_, iov, iovcnt, scratch, sizeof(scratch));
    }
  }
  if (sent > 0) {
    this->stall_.clear();
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
  this->stall_.start(now);
  if (!this->stall_.is_expired(now, SEND_STALL_TIMEOUT_MS)) {
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
  // Before the handoff there is no client yet; loop() closes it once attached
  if (this->close_sent_ || !this->is_attached() || !this->is_connected()) {
    return;
  }
  this->close_sent_ = true;
  LwIPLock lock;
  if (this->is_connected()) {
    this->client_->close(true);
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
