#include "microphone_http_stream.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/progmem.h"

#include <cinttypes>
#include <cstdio>
#include <sys/socket.h>

#ifdef USE_PSRAM
#include <esp_heap_caps.h>
#endif

namespace esphome::microphone_http_stream {

ESPHOME_LOG_TAG(TAG, "microphone_http_stream");

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "S16LE requires a little-endian target");

static constexpr char LAST_CHUNK[] = "0\r\n\r\n";

// Indexed by StopReason
PROGMEM_STRING_TABLE(StopReasonStrings, "client_closed", "buffer_full", "peer_closed", "send_failed", "audio_timeout");

void MicrophoneHttpStream::setup() {
  this->large_ring_ = this->clients_.size() > 1;
#ifdef USE_PSRAM
  // With PSRAM one client also gets the large ring, so it rides out Wi-Fi pauses of several seconds
  this->large_ring_ |= heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
#endif
  bool allocated = true;
  for (auto &client : this->clients_) {
    client.ring = ring_buffer::RingBuffer::create(this->large_ring_ ? MULTI_RING_SIZE : SINGLE_RING_SIZE);
    if (client.ring == nullptr && this->large_ring_ && this->clients_.size() == 1) {
      // PSRAM already used by other components; the small ring still streams over Ethernet
      ESP_LOGW(TAG, "Cannot allocate %u bytes in PSRAM, using %u", static_cast<unsigned>(MULTI_RING_SIZE),
               static_cast<unsigned>(SINGLE_RING_SIZE));
      this->large_ring_ = false;
      client.ring = ring_buffer::RingBuffer::create(SINGLE_RING_SIZE);
    }
    if (client.ring == nullptr) {
      allocated = false;
      break;
    }
  }
  if (allocated) {
    this->source_->add_data_callback(
        [this](const microphone::AudioCapture &capture) { this->on_audio_(capture.data); });
    this->disable_loop();
  } else {
    for (auto &client : this->clients_) {
      client.ring.reset();
    }
    this->mark_failed(LOG_STR("Cannot allocate microphone audio buffers"));
  }
  // Registered even after a failure, so requests get a 503 that says why the stream is unavailable
  this->base_->init();
  this->base_->add_handler(this);
}

void MicrophoneHttpStream::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Microphone HTTP Stream:\n  ID: %s\n  Format: S16LE mono, %" PRIu32
                " Hz\n  Path: %s\n  Maximum clients: %u\n  Buffer per client: %u B",
                this->stream_id_(), this->source_->get_audio_stream_info().get_sample_rate(), this->stream_path_,
                static_cast<unsigned>(this->clients_.size()),
                static_cast<unsigned>(this->large_ring_ ? MULTI_RING_SIZE : SINGLE_RING_SIZE));
}

bool MicrophoneHttpStream::canHandle(AsyncWebServerRequest *request) const {
  if (request->method() != HTTP_GET)
    return false;
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const StringRef url = request->url_to(url_buf);
  return url == LIST_PATH || url == this->stream_path_;
}

void MicrophoneHttpStream::handleRequest(AsyncWebServerRequest *request) {
  if (this->is_failed()) {
    request->send(503, "text/plain; charset=utf-8", "Microphone stream unavailable");
    return;
  }
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  if (request->url_to(url_buf) == LIST_PATH) {
    char json[256];
    snprintf(json, sizeof(json),
             "[{\"stream_id\":\"%s\",\"format\":\"S16LE\",\"sample_rate_hz\":%" PRIu32
             ",\"channels\":1,\"content_type\":\"application/octet-stream\",\"url\":\"%s\",\"max_clients\":%u}]",
             this->stream_id_(), this->source_->get_audio_stream_info().get_sample_rate(), this->stream_path_,
             static_cast<unsigned>(this->clients_.size()));
    request->send(200, "application/json", json);
    return;
  }

  ClientSlot *client = nullptr;
  for (auto &slot : this->clients_) {
    StreamState expected = StreamState::STREAM_STATE_IDLE;
    if (slot.state.compare_exchange_strong(expected, StreamState::STREAM_STATE_PREPARING)) {
      client = &slot;
      break;
    }
  }
  if (client == nullptr) {
    request->send(409, "text/plain; charset=utf-8", "No free microphone stream slot");
    return;
  }
  client->stop_reason.store(StopReason::STOP_REASON_NONE);
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
  for (auto &client : this->clients_) {
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

  // Both are no-ops when the source is already in that state
  if (has_ready_client) {
    this->source_->start();
  } else if (!has_pending_client) {
    this->source_->stop();
  }
  // A claimed slot calls enable_loop_soon_any_context() once it is READY.
  if (!has_ready_client && !has_pending_client && !has_finishing_client)
    this->disable_loop();
}

void MicrophoneHttpStream::service_client_(ClientSlot *client) {
  AsyncStreamResponse *stream = client->stream;
  if (!stream->is_closing()) {
    if (client->stop_reason.load() != StopReason::STOP_REASON_NONE) {
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
      web_server_base::write_chunk_header(client->chunk_header, amount);
    }

    struct iovec parts[3] = {
        {client->chunk_header, web_server_base::CHUNK_HDR_LEN},
        {client->chunk_data, client->chunk_len},
        {const_cast<char *>(web_server_base::CHUNK_END), web_server_base::CHUNK_END_LEN},
    };
    struct iovec *iov = parts;
    int iovcnt = 3;
    web_server_base::advance_iovecs(iov, iovcnt, client->chunk_sent);
    const ssize_t sent = stream->write(iov, iovcnt);
    if (sent < 0) {
      client->set_stop_reason(StopReason::STOP_REASON_SEND_FAILED);  // write() already closed the stream
      return;
    }
    client->chunk_sent += static_cast<uint16_t>(sent);
    if (client->chunk_sent < web_server_base::CHUNK_HDR_LEN + client->chunk_len + web_server_base::CHUNK_END_LEN)
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
  const LogString *reason =
      StopReasonStrings::get_log_str(static_cast<uint8_t>(stop_reason), StopReasonStrings::LAST_INDEX);
  const unsigned index = this->client_index_(client);
  if (stop_reason == StopReason::STOP_REASON_NONE || stop_reason == StopReason::STOP_REASON_PEER_CLOSED) {
    ESP_LOGI(TAG, "Client %u disconnected: reason=%s", index, LOG_STR_ARG(reason));
  } else {
    ESP_LOGW(TAG, "Client %u disconnected: reason=%s", index, LOG_STR_ARG(reason));
  }
  // loop() publishes IDLE once no audio callback is still writing to this slot
  client->state.store(StreamState::STREAM_STATE_FINISHING);
}

void MicrophoneHttpStream::on_audio_(std::span<const uint8_t> data) {
  if (data.empty())
    return;
  for (auto &client : this->clients_) {
    if (client.state.load() != StreamState::STREAM_STATE_READY)
      continue;
    client.writers.fetch_add(1);
    // Re-check once counted, so a slot that is finishing or reused is never written (seq_cst, see loop())
    if (client.state.load() == StreamState::STREAM_STATE_READY &&
        client.stop_reason.load() == StopReason::STOP_REASON_NONE)
      this->write_audio_(&client, data);
    client.writers.fetch_sub(1, std::memory_order_release);
  }
}

void MicrophoneHttpStream::write_audio_(ClientSlot *client, std::span<const uint8_t> data) {
  // Whole frames only, so the sender always reads complete 2-byte samples.
  if (client->ring->write_without_replacement(data.data(), data.size(), 0, false) == 0)
    client->set_stop_reason(StopReason::STOP_REASON_BUFFER_FULL);  // Disconnect only the client that cannot keep up.
}

}  // namespace esphome::microphone_http_stream

#endif  // USE_ESP32
