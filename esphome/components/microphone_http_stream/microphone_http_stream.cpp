#include "microphone_http_stream.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"

#include <cinttypes>
#include <cstdio>
#include <sys/socket.h>

namespace esphome::microphone_http_stream {

ESPHOME_LOG_TAG(TAG, "microphone_http_stream");

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "S16LE requires a little-endian target");

static constexpr StringRef LIST_PATH = StringRef::from_lit("/audio/microphones");
static constexpr char CRLF[] = "\r\n";
static constexpr char LAST_CHUNK[] = "0\r\n\r\n";

void MicrophoneHttpStream::setup() {
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    client.ring = ring_buffer::RingBuffer::create(this->ring_size_());
    if (client.ring == nullptr) {
      this->fail_setup_(LOG_STR("Cannot allocate microphone audio buffers"));
      return;
    }
  }
  this->source_->add_data_callback([this](const microphone::AudioCapture &capture) { this->on_audio_(capture.data); });
  this->register_handler_();
  this->disable_loop();
}

void MicrophoneHttpStream::fail_setup_(const LogString *reason) {
  for (auto &client : this->clients_) {
    client.ring.reset();
  }
  this->mark_failed(reason);
  // Still answer requests, with 503, so clients see why the stream is unavailable.
  this->register_handler_();
}

void MicrophoneHttpStream::register_handler_() {
  this->base_->init();
  this->base_->add_handler(this);
}

void MicrophoneHttpStream::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Microphone HTTP Stream:\n  ID: %s\n  Format: S16LE mono, %" PRIu32
                " Hz\n  Path: %s\n  Maximum clients: %u\n  Buffer per client: %u B",
                this->stream_id_(), this->source_->get_audio_stream_info().get_sample_rate(),
                this->stream_path_.c_str(), this->max_clients_, static_cast<unsigned>(this->ring_size_()));
}

bool MicrophoneHttpStream::canHandle(AsyncWebServerRequest *request) const {
  if (request->method() != HTTP_GET)
    return false;
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const StringRef url = request->url_to(url_buf);
  return url == LIST_PATH || url == this->stream_path_;
}

void MicrophoneHttpStream::fail_request_(AsyncWebServerRequest *request, const char *message) {
  ESP_LOGW(TAG, "Stream request failed: %s", message);
  request->send(503, "text/plain; charset=utf-8", message);
}

void MicrophoneHttpStream::handleRequest(AsyncWebServerRequest *request) {
  if (this->is_failed()) {
    this->fail_request_(request, "Microphone stream unavailable");
    return;
  }
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  if (request->url_to(url_buf) == LIST_PATH) {
    char json[256];
    snprintf(json, sizeof(json),
             "[{\"stream_id\":\"%s\",\"format\":\"S16LE\",\"sample_rate_hz\":%" PRIu32
             ",\"channels\":1,\"content_type\":\"application/octet-stream\",\"url\":\"%s\",\"max_clients\":%u}]",
             this->stream_id_(), this->source_->get_audio_stream_info().get_sample_rate(), this->stream_path_.c_str(),
             this->max_clients_);
    request->send(200, "application/json", json);
    return;
  }

  ClientSlot *client = nullptr;
  for (size_t i = 0; i < this->max_clients_; ++i) {
    StreamState expected = StreamState::STREAM_STATE_IDLE;
    if (this->clients_[i].state.compare_exchange_strong(expected, StreamState::STREAM_STATE_PREPARING)) {
      client = &this->clients_[i];
      break;
    }
  }
  if (client == nullptr) {
    request->send(409, "text/plain; charset=utf-8", "No free microphone stream slot");
    return;
  }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  client->peak_ring_used.store(0);
#endif
  client->stop_reason.store(StopReason::STOP_REASON_NONE);
  client->abort.store(0);
  client->stalled = false;
  // Drop audio left over from the previous client of this slot.
  client->ring->reset();

  char sample_rate[12];
  snprintf(sample_rate, sizeof(sample_rate), "%" PRIu32, this->source_->get_audio_stream_info().get_sample_rate());
  const HttpHeader headers[] = {
      {"Transfer-Encoding", "chunked"}, {"X-Audio-Format", "S16LE"},   {"X-Audio-Sample-Rate", sample_rate},
      {"X-Audio-Channels", "1"},        {"Cache-Control", "no-store"}, {"Connection", "close"},
  };
  // Deleted by the main loop once safe_to_delete(), like the SSE sessions.
  auto *stream = new AsyncStreamResponse();  // NOLINT(cppcoreguidelines-owning-memory)
  stream->begin(request, HTTPD_200, "application/octet-stream", headers);
  client->stream = stream;
  client->last_audio_ms = 0;
  client->state.store(StreamState::STREAM_STATE_READY);
  this->enable_loop_soon_any_context();
  ESP_LOGI(TAG, "Client %u connected", this->client_index_(client));
}

void MicrophoneHttpStream::loop() {
  bool has_ready_client = false;
  bool has_pending_client = false;
  bool has_finishing_client = false;
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    auto state = client.state.load();
    if (state == StreamState::STREAM_STATE_READY) {
      this->service_client_(&client);
      state = client.state.load();  // The client may have just finished
    }
    if (state == StreamState::STREAM_STATE_FINISHING) {
      // seq_cst pairs with on_audio_(): a writer either is counted here or sees FINISHING and skips the slot.
      if (client.writers.load() == 0) {
        client.state.store(StreamState::STREAM_STATE_IDLE);
      } else {
        has_finishing_client = true;
      }
    }
    has_ready_client |= state == StreamState::STREAM_STATE_READY;
    has_pending_client |= state == StreamState::STREAM_STATE_PREPARING;
  }

  if (has_ready_client && !this->streaming_active_) {
    this->streaming_active_ = true;
    this->source_->start();
  } else if (!has_ready_client && !has_pending_client && this->streaming_active_) {
    ESP_LOGI(TAG, "Last HTTP client left; stopping microphone source");
    this->source_->stop();
    this->streaming_active_ = false;
  }
  // A claimed slot calls enable_loop_soon_any_context() once it is READY.
  if (!has_ready_client && !has_pending_client && !has_finishing_client)
    this->disable_loop();
}

void MicrophoneHttpStream::service_client_(ClientSlot *client) {
  AsyncStreamResponse *stream = client->stream;
  if (!stream->is_closing()) {
    if (client->abort.load()) {
      stream->close();  // The audio callback recorded the reason
    } else if (!stream->is_connected()) {
      client->set_stop_reason(StopReason::STOP_REASON_PEER_CLOSED);
      stream->close();
    } else if (stream->is_attached()) {
      this->send_audio_(client);
    }
  }
  stream->loop();
  if (stream->is_closing() && stream->safe_to_delete())
    this->finish_client_(client);
}

void MicrophoneHttpStream::send_audio_(ClientSlot *client) {
  AsyncStreamResponse *stream = client->stream;
  const uint32_t now = App.get_loop_component_start_time();
  // A stamp that happens to be 0 is only re-taken on the next pass
  if (client->last_audio_ms == 0)
    client->last_audio_ms = now;
  while (true) {
    if (client->chunk_data == nullptr) {
      size_t amount = 0;
      // Writes are whole frames and the ring size is even, so every chunk holds complete samples.
      void *data = client->ring->receive_acquire(amount, SEND_CHUNK_SIZE);
      if (data == nullptr) {
        if (now - client->last_audio_ms >= AUDIO_WAIT_TIMEOUT_MS) {
          client->set_stop_reason(StopReason::STOP_REASON_AUDIO_TIMEOUT);
          stream->write(LAST_CHUNK, sizeof(LAST_CHUNK) - 1);  // Best effort; the stream closes either way
          stream->close();
        }
        return;
      }
      client->last_audio_ms = now;
      client->chunk_data = data;
      client->chunk_len = static_cast<uint16_t>(amount);
      client->chunk_sent = 0;
      client->chunk_header_len = static_cast<uint8_t>(
          snprintf(client->chunk_header, sizeof(client->chunk_header), "%x\r\n", static_cast<unsigned>(amount)));
    }

    // Gather the unsent tail of chunk_header + data + CRLF
    const size_t parts_len[3] = {client->chunk_header_len, client->chunk_len, sizeof(CRLF) - 1};
    const char *parts[3] = {client->chunk_header, static_cast<const char *>(client->chunk_data), CRLF};
    struct iovec iov[3];
    int iovcnt = 0;
    size_t skip = client->chunk_sent;
    size_t remaining = 0;
    for (size_t i = 0; i < 3; ++i) {
      if (skip >= parts_len[i]) {
        skip -= parts_len[i];
        continue;
      }
      iov[iovcnt].iov_base = const_cast<char *>(parts[i] + skip);
      iov[iovcnt].iov_len = parts_len[i] - skip;
      remaining += iov[iovcnt].iov_len;
      ++iovcnt;
      skip = 0;
    }

    const ssize_t sent = stream->write(iov, iovcnt);
    if (sent < 0) {
      // write() already closed the stream; a refusal right before means it stalled
      client->set_stop_reason(client->stalled ? StopReason::STOP_REASON_SEND_TIMEOUT
                                              : StopReason::STOP_REASON_SEND_FAILED);
      return;
    }
    client->stalled = sent == 0;
    if (sent == 0)
      return;
    client->chunk_sent += static_cast<uint16_t>(sent);
    if (static_cast<size_t>(sent) < remaining)
      return;  // Socket full; continue from chunk_sent on the next pass
    client->ring->receive_release(client->chunk_data);
    client->chunk_data = nullptr;
  }
}

void MicrophoneHttpStream::finish_client_(ClientSlot *client) {
  if (client->chunk_data != nullptr) {
    client->ring->receive_release(client->chunk_data);
    client->chunk_data = nullptr;
  }
  delete client->stream;  // NOLINT(cppcoreguidelines-owning-memory)
  client->stream = nullptr;
  const StopReason stop_reason = client->stop_reason.load();
  const LogString *reason = stop_reason_name(stop_reason);
  const unsigned index = this->client_index_(client);
  if (stop_reason == StopReason::STOP_REASON_NONE || stop_reason == StopReason::STOP_REASON_PEER_CLOSED) {
    ESP_LOGI(TAG, "Client %u disconnected: reason=%s", index, LOG_STR_ARG(reason));
  } else {
    ESP_LOGW(TAG, "Client %u disconnected: reason=%s", index, LOG_STR_ARG(reason));
  }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  ESP_LOGD(TAG, "Client %u diagnostics: peak_ring=%u", index,
           static_cast<unsigned>(client->peak_ring_used.load(std::memory_order_relaxed)));
#endif
  // loop() publishes IDLE once no audio callback is still writing to this slot
  client->state.store(StreamState::STREAM_STATE_FINISHING);
}

void MicrophoneHttpStream::on_audio_(std::span<const uint8_t> data) {
  if (data.empty())
    return;
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    if (client.state.load() != StreamState::STREAM_STATE_READY)
      continue;
    client.writers.fetch_add(1);
    // Re-check once counted, so a slot that is finishing or reused is never written (seq_cst, see loop())
    if (client.state.load() == StreamState::STREAM_STATE_READY && !client.abort.load())
      this->write_audio_(&client, data);
    client.writers.fetch_sub(1, std::memory_order_release);
  }
}

void MicrophoneHttpStream::write_audio_(ClientSlot *client, std::span<const uint8_t> data) {
  if ((data.size() & 1U) != 0) {
    client->stop(StopReason::STOP_REASON_INVALID_FRAME);
    return;
  }
  // Whole frames only, so the sender always reads complete 2-byte samples.
  if (client->ring->write_without_replacement(data.data(), data.size(), 0, false) == 0) {
    client->stop(StopReason::STOP_REASON_BUFFER_FULL);  // Disconnect only the client that cannot keep up.
    return;
  }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  const uint32_t filled = this->ring_size_() - client->ring->free();
  if (filled > client->peak_ring_used.load(std::memory_order_relaxed))
    client->peak_ring_used.store(filled, std::memory_order_relaxed);
#endif
}

const LogString *MicrophoneHttpStream::stop_reason_name(StopReason reason) {
  switch (reason) {
    case StopReason::STOP_REASON_NONE:
      return LOG_STR("client_closed");
    case StopReason::STOP_REASON_BUFFER_FULL:
      return LOG_STR("buffer_full");
    case StopReason::STOP_REASON_INVALID_FRAME:
      return LOG_STR("invalid_frame");
    case StopReason::STOP_REASON_PEER_CLOSED:
      return LOG_STR("peer_closed");
    case StopReason::STOP_REASON_SEND_TIMEOUT:
      return LOG_STR("send_timeout");
    case StopReason::STOP_REASON_SEND_FAILED:
      return LOG_STR("send_failed");
    case StopReason::STOP_REASON_AUDIO_TIMEOUT:
      return LOG_STR("audio_timeout");
  }
  return LOG_STR("unknown");
}

}  // namespace esphome::microphone_http_stream

#endif  // USE_ESP32
