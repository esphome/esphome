#include "microphone_http_stream.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>
#include <sys/socket.h>

namespace esphome::microphone_http_stream {

ESPHOME_LOG_TAG(TAG, "microphone_http_stream");

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "S16LE requires a little-endian target");

void MicrophoneHttpStream::setup() {
  const auto info = this->source_->get_audio_stream_info();
  if (info.get_bits_per_sample() != 16 || info.get_channels() != 1 || info.get_sample_rate() == 0) {
    this->mark_failed(LOG_STR("Source must provide mono 16-bit PCM with a known sample rate"));
    return;
  }
  this->sample_rate_ = info.get_sample_rate();
  snprintf(this->sample_rate_header_, sizeof(this->sample_rate_header_), "%" PRIu32, this->sample_rate_);
  snprintf(this->stream_path_, sizeof(this->stream_path_), "/audio/microphones/%s", this->stream_id_);

  this->control_mutex_ = xSemaphoreCreateMutex();
  if (this->control_mutex_ == nullptr) {
    this->mark_failed(LOG_STR("Cannot allocate microphone stream mutex"));
    return;
  }
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    client.owner = this;
    client.index = i;
    client.data_ready = xSemaphoreCreateBinary();
    if (client.data_ready == nullptr) {
      for (size_t j = 0; j < i; ++j) {
        vSemaphoreDelete(this->clients_[j].data_ready);
        this->clients_[j].data_ready = nullptr;
      }
      vSemaphoreDelete(this->control_mutex_);
      this->control_mutex_ = nullptr;
      this->mark_failed(LOG_STR("Cannot allocate microphone client notification"));
      return;
    }
  }
  this->source_->add_data_callback([this](const std::vector<uint8_t> &data) { this->on_audio_(data); });
  this->base_->init();
  this->base_->add_handler(this);
}

void MicrophoneHttpStream::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Microphone HTTP Stream:\n  ID: %s\n  Format: S16LE mono, %" PRIu32
                " Hz\n  Path: %s\n  Maximum clients: %u\n  Buffer per client: %u B",
                this->stream_id_, this->sample_rate_, this->stream_path_, this->max_clients_,
                static_cast<unsigned>(this->ring_size_));
}

bool MicrophoneHttpStream::canHandle(AsyncWebServerRequest *request) const {
  if (request->method() != HTTP_GET)
    return false;
  char url[AsyncWebServerRequest::URL_BUF_SIZE];
  request->url_to(url);
  return strcmp(url, "/audio/microphones") == 0 || strcmp(url, this->stream_path_) == 0;
}

void MicrophoneHttpStream::fail_request_(AsyncWebServerRequest *request, const char *message) {
  httpd_resp_set_status(*request, "503 Service Unavailable");
  httpd_resp_set_type(*request, "text/plain; charset=utf-8");
  httpd_resp_send(*request, message, HTTPD_RESP_USE_STRLEN);
}

void MicrophoneHttpStream::handleRequest(AsyncWebServerRequest *request) {
  char url[AsyncWebServerRequest::URL_BUF_SIZE];
  request->url_to(url);
  if (strcmp(url, "/audio/microphones") == 0) {
    char json[256];
    snprintf(json, sizeof(json),
             "[{\"stream_id\":\"%s\",\"format\":\"S16LE\",\"sample_rate_hz\":%" PRIu32
             ",\"channels\":1,\"content_type\":\"application/octet-stream\",\"url\":\"%s\",\"max_clients\":%u}]",
             this->stream_id_, this->sample_rate_, this->stream_path_, this->max_clients_);
    request->send(200, "application/json", json);
    return;
  }

  ClientSlot *client = nullptr;
  xSemaphoreTake(this->control_mutex_, portMAX_DELAY);
  for (size_t i = 0; i < this->max_clients_; ++i) {
    if (this->clients_[i].state.load() == StreamState::IDLE) {
      client = &this->clients_[i];
      client->generation.fetch_add(1);
      client->state.store(StreamState::PREPARING);
      break;
    }
  }
  xSemaphoreGive(this->control_mutex_);
  if (client == nullptr) {
    request->send(409, "text/plain; charset=utf-8", "No free microphone stream slot");
    return;
  }
  client->ring = static_cast<uint8_t *>(heap_caps_malloc(this->ring_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (client->ring == nullptr && this->max_clients_ == 1)
    client->ring = static_cast<uint8_t *>(heap_caps_malloc(this->ring_size_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (client->ring == nullptr) {
    client->state.store(StreamState::IDLE);
    this->fail_request_(request, "Cannot allocate audio buffer");
    return;
  }
  client->read_seq.store(0);
  client->write_seq.store(0);
  client->producer_active.store(false);
  xSemaphoreTake(client->data_ready, 0);
  client->abort.store(false);
  client->socket_fd = httpd_req_to_sockfd(*request);

  if (httpd_req_async_handler_begin(*request, &client->async_request) != ESP_OK) {
    heap_caps_free(client->ring);
    client->ring = nullptr;
    client->state.store(StreamState::IDLE);
    this->fail_request_(request, "Cannot start asynchronous HTTP response");
    return;
  }
  if (xTaskCreate(&MicrophoneHttpStream::send_task_, "mic_http_send", SEND_TASK_STACK_SIZE, client, 2, nullptr) !=
      pdPASS) {
    httpd_resp_set_status(client->async_request, "503 Service Unavailable");
    httpd_resp_send(client->async_request, "Cannot create stream task", HTTPD_RESP_USE_STRLEN);
    httpd_req_async_handler_complete(client->async_request);
    client->async_request = nullptr;
    heap_caps_free(client->ring);
    client->ring = nullptr;
    client->state.store(StreamState::IDLE);
    return;
  }
  client->state.store(StreamState::READY);
  ESP_LOGI(TAG, "Client %u connected", client->index);
}

void MicrophoneHttpStream::loop() {
  bool has_ready_client = false;
  bool has_pending_client = false;
  xSemaphoreTake(this->control_mutex_, portMAX_DELAY);
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    const auto state = client.state.load();
    if (state == StreamState::FINISHED) {
      if (!client.producer_active.load()) {
        heap_caps_free(client.ring);
        client.ring = nullptr;
        client.read_seq.store(0);
        client.write_seq.store(0);
        client.async_request = nullptr;
        client.socket_fd = -1;
        client.state.store(StreamState::IDLE);
      }
    } else if (state == StreamState::READY) {
      has_ready_client = true;
    } else if (state == StreamState::PREPARING) {
      has_pending_client = true;
    }
  }

  if (has_ready_client && !this->capture_started_) {
    if (!this->microphone_->is_stopped()) {
      ESP_LOGW(TAG, "Microphone already active; ending HTTP clients");
      for (size_t i = 0; i < this->max_clients_; ++i) {
        auto &client = this->clients_[i];
        if (client.state.load() == StreamState::READY) {
          client.abort.store(true);
          xSemaphoreGive(client.data_ready);
        }
      }
    } else {
      this->capture_started_ = true;
      this->source_->start();
    }
  } else if (!has_ready_client && !has_pending_client && this->capture_started_) {
    ESP_LOGI(TAG, "Last HTTP client left; stopping microphone");
    this->source_->stop();
    this->capture_started_ = false;
  }
  xSemaphoreGive(this->control_mutex_);
}

void MicrophoneHttpStream::on_audio_(const std::vector<uint8_t> &data) {
  if (data.empty())
    return;
  for (size_t i = 0; i < this->max_clients_; ++i) {
    auto &client = this->clients_[i];
    const uint32_t generation = client.generation.load();
    if (client.state.load() != StreamState::READY || client.abort.load())
      continue;
    bool expected = false;
    if (!client.producer_active.compare_exchange_strong(expected, true)) {
      client.abort.store(true);
      xSemaphoreGive(client.data_ready);
      ESP_LOGW(TAG, "Client %u received overlapping audio callbacks", client.index);
      continue;
    }
    // The sender only advances read_seq after copying into its task-local chunk.
    if (client.state.load() == StreamState::READY && client.generation.load() == generation && !client.abort.load()) {
      const size_t write = client.write_seq.load();
      const size_t read = client.read_seq.load();
      const size_t used = write - read;
      if ((data.size() & 1U) != 0 || used > this->ring_size_ || data.size() > this->ring_size_ - used) {
        client.abort.store(true);  // Disconnect only the client that cannot keep up.
        ESP_LOGW(TAG, "Client %u ring full or incomplete frame: used=%u incoming=%u", client.index,
                 static_cast<unsigned>(used), static_cast<unsigned>(data.size()));
      } else {
        const size_t pos = write % this->ring_size_;
        const size_t first = std::min(data.size(), this->ring_size_ - pos);
        memcpy(client.ring + pos, data.data(), first);
        memcpy(client.ring, data.data() + first, data.size() - first);
        client.write_seq.store(write + data.size());
      }
      xSemaphoreGive(client.data_ready);
    }
    client.producer_active.store(false);
  }
}

void MicrophoneHttpStream::send_task_(void *arg) {
  auto *client = static_cast<ClientSlot *>(arg);
  client->owner->run_send_task_(client);
  vTaskDelete(nullptr);
}

void MicrophoneHttpStream::run_send_task_(ClientSlot *client) {
  httpd_req_t *request = client->async_request;
  httpd_resp_set_type(request, "application/octet-stream");
  httpd_resp_set_hdr(request, "X-Audio-Format", "S16LE");
  httpd_resp_set_hdr(request, "X-Audio-Sample-Rate", this->sample_rate_header_);
  httpd_resp_set_hdr(request, "X-Audio-Channels", "1");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "Connection", "close");

  uint8_t chunk[SEND_CHUNK_SIZE];
  uint32_t last_audio_ms = millis();
  bool send_ok = true;
  bool response_started = false;
  while (!client->abort.load()) {
    size_t amount = 0;
    if (client->state.load() == StreamState::READY) {
      const size_t read = client->read_seq.load();
      const size_t write = client->write_seq.load();
      amount = std::min(write - read, SEND_CHUNK_SIZE);
      // The source is mono S16LE, so each HTTP chunk must contain complete 2-byte frames.
      amount &= ~size_t(1);
      const size_t pos = read % this->ring_size_;
      const size_t first = std::min(amount, this->ring_size_ - pos);
      memcpy(chunk, client->ring + pos, first);
      memcpy(chunk + first, client->ring, amount - first);
      client->read_seq.store(read + amount);
    }
    if (amount != 0) {
      last_audio_ms = millis();
      response_started = true;
      const auto result = httpd_resp_send_chunk(request, reinterpret_cast<const char *>(chunk), amount);
      if (result != ESP_OK) {
        ESP_LOGW(TAG, "Client %u HTTP send failed: %d errno=%d", client->index, result, errno);
        send_ok = false;
        break;
      }
    } else {
      if (millis() - last_audio_ms >= AUDIO_WAIT_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Client %u audio timed out", client->index);
        break;
      }
      xSemaphoreTake(client->data_ready, pdMS_TO_TICKS(100));
    }
  }
  if (!response_started) {
    httpd_resp_set_status(request, "503 Service Unavailable");
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    httpd_resp_send(request, "Microphone produced no audio", HTTPD_RESP_USE_STRLEN);
  } else if (send_ok && !client->abort.load()) {
    httpd_resp_send_chunk(request, nullptr, 0);
  }
  // The HTTP server owns the descriptor. Shutdown requests a close without reusing or freeing its fd here.
  if (client->socket_fd >= 0)
    shutdown(client->socket_fd, SHUT_RDWR);
  httpd_req_async_handler_complete(request);
  ESP_LOGI(TAG, "Client %u disconnected", client->index);
  client->state.store(StreamState::FINISHED);
}

}  // namespace esphome::microphone_http_stream
