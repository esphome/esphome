#include "microphone_http_stream.h"

#ifdef USE_ESP32

#include "esphome/components/web_server/web_server.h"

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <sys/socket.h>
#include <sys/time.h>

namespace esphome::microphone_http_stream {

ESPHOME_LOG_TAG(TAG, "microphone_http_stream");

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "S16LE requires a little-endian target");

static constexpr StringRef LIST_PATH = StringRef::from_lit("/audio/microphones");

void MicrophoneHttpStream::setup() {
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    client.owner = this;
    client.ring = ring_buffer::RingBuffer::create(this->ring_size_());
    if (client.ring == nullptr) {
      this->fail_setup_(LOG_STR("Cannot allocate microphone audio buffers"));
      return;
    }
    if (xTaskCreate(&MicrophoneHttpStream::send_task, "mic_http_send", SEND_TASK_STACK_SIZE, &client, 2,
                    &client.task) != pdPASS) {
      this->fail_setup_(LOG_STR("Cannot create microphone stream task"));
      return;
    }
  }
  this->source_->add_data_callback([this](const microphone::AudioCapture &capture) { this->on_audio_(capture.data); });
  this->register_handler_();
  this->disable_loop();
}

void MicrophoneHttpStream::fail_setup_(const LogString *reason) {
  for (auto &client : this->clients_) {
    if (client.task != nullptr) {
      vTaskDelete(client.task);
      client.task = nullptr;
    }
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

void MicrophoneHttpStream::release_slot_(ClientSlot *client) {
  client->state.store(StreamState::STREAM_STATE_IDLE);
  // The loop may have kept the source running for this pending client.
  this->enable_loop_soon_any_context();
}

void MicrophoneHttpStream::handleRequest(AsyncWebServerRequest *request) {
  if (!this->server_->is_request_origin_allowed(request)) {
    request->send(403);
    return;
  }
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
  const int socket_fd = httpd_req_to_sockfd(*request);
  timeval send_timeout{};
  send_timeout.tv_sec = SEND_TIMEOUT_SEC;
  if (socket_fd < 0 || setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) != 0) {
    this->release_slot_(client);
    this->fail_request_(request, "Cannot set audio send timeout");
    return;
  }
  if (httpd_req_async_handler_begin(*request, &client->async_request) != ESP_OK) {
    this->release_slot_(client);
    this->fail_request_(request, "Cannot start asynchronous HTTP response");
    return;
  }
  client->state.store(StreamState::STREAM_STATE_READY);
  xTaskNotifyGive(client->task);
  this->enable_loop_soon_any_context();
  ESP_LOGI(TAG, "Client %u connected", this->client_index_(client));
}

void MicrophoneHttpStream::loop() {
  bool has_ready_client = false;
  bool has_pending_client = false;
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    const auto state = client.state.load();
    if (state == StreamState::STREAM_STATE_FINISHED) {
      client.state.store(StreamState::STREAM_STATE_IDLE);
    } else if (state == StreamState::STREAM_STATE_READY) {
      has_ready_client = true;
    } else if (state == StreamState::STREAM_STATE_PREPARING) {
      has_pending_client = true;
    }
  }

  if (has_ready_client && !this->streaming_active_) {
    this->streaming_active_ = true;
    this->source_->start();
  } else if (!has_ready_client && !has_pending_client && this->streaming_active_) {
    ESP_LOGI(TAG, "Last HTTP client left; stopping microphone source");
    this->source_->stop();
    this->streaming_active_ = false;
  }
  // Every move to READY, FINISHED or back to IDLE calls enable_loop_soon_any_context().
  this->disable_loop();
}

void MicrophoneHttpStream::on_audio_(std::span<const uint8_t> data) {
  if (data.empty())
    return;
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    if (client.state.load() != StreamState::STREAM_STATE_READY || client.abort.load())
      continue;
    if ((data.size() & 1U) != 0) {
      client.stop(StopReason::STOP_REASON_INVALID_FRAME);
      continue;
    }
    // Whole frames only, so the sender always reads complete 2-byte samples.
    if (client.ring->write_without_replacement(data.data(), data.size(), 0, false) == 0) {
      client.stop(StopReason::STOP_REASON_BUFFER_FULL);  // Disconnect only the client that cannot keep up.
      continue;
    }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
    const uint32_t filled = this->ring_size_() - client.ring->free();
    if (filled > client.peak_ring_used.load(std::memory_order_relaxed))
      client.peak_ring_used.store(filled, std::memory_order_relaxed);
#endif
  }
}

void MicrophoneHttpStream::send_task(void *arg) {
  auto *client = static_cast<ClientSlot *>(arg);
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    client->owner->stream_to_client_(client);
  }
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

void MicrophoneHttpStream::stream_to_client_(ClientSlot *client) {
  httpd_req_t *request = client->async_request;
  // httpd keeps the header pointer until the first chunk is sent.
  char sample_rate[12];
  snprintf(sample_rate, sizeof(sample_rate), "%" PRIu32, this->source_->get_audio_stream_info().get_sample_rate());
  httpd_resp_set_type(request, "application/octet-stream");
  httpd_resp_set_hdr(request, "X-Audio-Format", "S16LE");
  httpd_resp_set_hdr(request, "X-Audio-Sample-Rate", sample_rate);
  httpd_resp_set_hdr(request, "X-Audio-Channels", "1");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "Connection", "close");

  // Drop audio left over from the previous client of this slot.
  client->ring->reset();
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  uint32_t max_send_ms = 0;
#endif
  uint32_t last_audio_ms = millis();
  bool send_ok = true;
  bool response_started = false;
  int send_errno = 0;
  while (!client->abort.load()) {
    size_t amount = 0;
    // Writes are whole frames and the ring size is even, so every chunk holds complete samples.
    void *data = client->ring->receive_acquire(amount, SEND_CHUNK_SIZE, pdMS_TO_TICKS(AUDIO_POLL_MS));
    if (data == nullptr) {
      if (millis() - last_audio_ms >= AUDIO_WAIT_TIMEOUT_MS) {
        client->set_stop_reason(StopReason::STOP_REASON_AUDIO_TIMEOUT);
        break;
      }
      continue;
    }
    last_audio_ms = millis();
    response_started = true;
    const auto result = httpd_resp_send_chunk(request, static_cast<const char *>(data), amount);
    client->ring->receive_release(data);
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
    max_send_ms = std::max(max_send_ms, millis() - last_audio_ms);
#endif
    if (result != ESP_OK) {
      send_errno = errno;
      StopReason failure_reason = StopReason::STOP_REASON_SEND_FAILED;
      if (send_errno == ECONNRESET || send_errno == EPIPE || send_errno == ENOTCONN) {
        failure_reason = StopReason::STOP_REASON_PEER_CLOSED;
      } else if (send_errno == EAGAIN || send_errno == EWOULDBLOCK) {
        failure_reason = StopReason::STOP_REASON_SEND_TIMEOUT;
      }
      client->set_stop_reason(failure_reason);
      send_ok = false;
      break;
    }
  }
  const StopReason stop_reason = client->stop_reason.load();
  const LogString *reason = stop_reason_name(stop_reason);
  if (!response_started) {
    httpd_resp_set_status(request, "503 Service Unavailable");
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    char message[64];
    snprintf(message, sizeof(message), "Microphone stream stopped: %s", LOG_STR_ARG(reason));
    httpd_resp_send(request, message, HTTPD_RESP_USE_STRLEN);
  } else if (send_ok && !client->abort.load()) {
    httpd_resp_send_chunk(request, nullptr, 0);
  }
  // The HTTP server owns the descriptor. Shutdown requests a close without reusing or freeing its fd here.
  const int socket_fd = httpd_req_to_sockfd(request);
  if (socket_fd >= 0)
    shutdown(socket_fd, SHUT_RDWR);
  httpd_req_async_handler_complete(request);
  client->async_request = nullptr;
  const unsigned index = this->client_index_(client);
  if (stop_reason == StopReason::STOP_REASON_NONE || stop_reason == StopReason::STOP_REASON_PEER_CLOSED) {
    ESP_LOGI(TAG, "Client %u disconnected: reason=%s", index, LOG_STR_ARG(reason));
  } else if (stop_reason == StopReason::STOP_REASON_SEND_TIMEOUT ||
             stop_reason == StopReason::STOP_REASON_SEND_FAILED) {
    ESP_LOGW(TAG, "Client %u disconnected: reason=%s errno=%d", index, LOG_STR_ARG(reason), send_errno);
  } else {
    ESP_LOGW(TAG, "Client %u disconnected: reason=%s", index, LOG_STR_ARG(reason));
  }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  ESP_LOGD(TAG, "Client %u diagnostics: peak_ring=%u max_send_ms=%u", index,
           static_cast<unsigned>(client->peak_ring_used.load(std::memory_order_relaxed)),
           static_cast<unsigned>(max_send_ms));
#endif
  client->state.store(StreamState::STREAM_STATE_FINISHED);
  this->enable_loop_soon_any_context();
}

}  // namespace esphome::microphone_http_stream

#endif  // USE_ESP32
