#include "microphone_http_stream.h"

#include "esphome/core/log.h"

#include <algorithm>
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

  this->ring_mutex_ = xSemaphoreCreateMutex();
  if (this->ring_mutex_ == nullptr) {
    this->mark_failed(LOG_STR("Cannot allocate microphone stream mutex"));
    return;
  }
  this->source_->add_data_callback([this](const std::vector<uint8_t> &data) { this->on_audio_(data); });
  this->base_->init();
  this->base_->add_handler(this);
  this->disable_loop();
}

void MicrophoneHttpStream::dump_config() {
  ESP_LOGCONFIG(TAG, "Microphone HTTP Stream:\n  ID: %s\n  Format: S16LE mono, %" PRIu32 " Hz\n  Path: %s",
                this->stream_id_, this->sample_rate_, this->stream_path_);
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
             ",\"channels\":1,\"content_type\":\"application/octet-stream\",\"url\":\"%s\"}]",
             this->stream_id_, this->sample_rate_, this->stream_path_);
    request->send(200, "application/json", json);
    return;
  }

  StreamState expected = StreamState::IDLE;
  if (!this->state_.compare_exchange_strong(expected, StreamState::PREPARING)) {
    request->send(409, "text/plain; charset=utf-8", "Microphone stream already has a client");
    return;
  }
  this->ring_ = static_cast<uint8_t *>(heap_caps_malloc(RING_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (this->ring_ == nullptr)
    this->ring_ = static_cast<uint8_t *>(heap_caps_malloc(RING_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (this->ring_ == nullptr) {
    this->state_.store(StreamState::IDLE);
    this->fail_request_(request, "Cannot allocate audio buffer");
    return;
  }
  this->read_pos_ = this->write_pos_ = this->used_ = 0;
  this->abort_.store(false);
  this->socket_fd_ = httpd_req_to_sockfd(*request);

  if (httpd_req_async_handler_begin(*request, &this->async_request_) != ESP_OK) {
    heap_caps_free(this->ring_);
    this->ring_ = nullptr;
    this->state_.store(StreamState::IDLE);
    this->fail_request_(request, "Cannot start asynchronous HTTP response");
    return;
  }
  if (xTaskCreate(&MicrophoneHttpStream::send_task_, "mic_http_send", SEND_TASK_STACK_SIZE, this, 2,
                  &this->send_task_handle_) != pdPASS) {
    httpd_resp_set_status(this->async_request_, "503 Service Unavailable");
    httpd_resp_send(this->async_request_, "Cannot create stream task", HTTPD_RESP_USE_STRLEN);
    httpd_req_async_handler_complete(this->async_request_);
    this->async_request_ = nullptr;
    heap_caps_free(this->ring_);
    this->ring_ = nullptr;
    this->state_.store(StreamState::IDLE);
    return;
  }
  this->state_.store(StreamState::READY);
  this->enable_loop_soon_any_context();
}

void MicrophoneHttpStream::loop() {
  const auto state = this->state_.load();
  if (state == StreamState::READY && !this->capture_started_) {
    if (!this->microphone_->is_stopped()) {
      this->abort_.store(true);
    } else {
      this->capture_started_ = true;
      this->source_->start();
    }
  } else if (state == StreamState::FINISHED) {
    if (this->capture_started_) {
      this->source_->stop();
      this->capture_started_ = false;
    }
    xSemaphoreTake(this->ring_mutex_, portMAX_DELAY);
    heap_caps_free(this->ring_);
    this->ring_ = nullptr;
    this->read_pos_ = this->write_pos_ = this->used_ = 0;
    xSemaphoreGive(this->ring_mutex_);
    this->send_task_handle_ = nullptr;
    this->async_request_ = nullptr;
    this->socket_fd_ = -1;
    this->state_.store(StreamState::IDLE);
    this->disable_loop();
  }
}

void MicrophoneHttpStream::on_audio_(const std::vector<uint8_t> &data) {
  if (this->state_.load() != StreamState::READY || this->abort_.load() || data.empty())
    return;
  if (!xSemaphoreTake(this->ring_mutex_, 0)) {
    this->abort_.store(true);
    return;
  }
  // A sender can finish after the first state check. Cleanup takes the same mutex before freeing the ring.
  if (this->state_.load() != StreamState::READY || this->ring_ == nullptr) {
    xSemaphoreGive(this->ring_mutex_);
    return;
  }
  if ((data.size() & 1U) != 0 || data.size() > RING_SIZE - this->used_) {
    this->abort_.store(true);  // Never drop a sample silently: disconnect this client.
  } else {
    const size_t first = std::min(data.size(), RING_SIZE - this->write_pos_);
    memcpy(this->ring_ + this->write_pos_, data.data(), first);
    memcpy(this->ring_, data.data() + first, data.size() - first);
    this->write_pos_ = (this->write_pos_ + data.size()) % RING_SIZE;
    this->used_ += data.size();
  }
  xTaskNotifyGive(this->send_task_handle_);
  xSemaphoreGive(this->ring_mutex_);
}

void MicrophoneHttpStream::send_task_(void *arg) {
  static_cast<MicrophoneHttpStream *>(arg)->run_send_task_();
  vTaskDelete(nullptr);
}

void MicrophoneHttpStream::run_send_task_() {
  httpd_req_t *request = this->async_request_;
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
  while (!this->abort_.load()) {
    size_t amount = 0;
    if (this->state_.load() == StreamState::READY && xSemaphoreTake(this->ring_mutex_, pdMS_TO_TICKS(10))) {
      amount = std::min(this->used_, SEND_CHUNK_SIZE);
      // The source is mono S16LE, so each HTTP chunk must contain complete 2-byte frames.
      amount &= ~size_t(1);
      const size_t first = std::min(amount, RING_SIZE - this->read_pos_);
      memcpy(chunk, this->ring_ + this->read_pos_, first);
      memcpy(chunk + first, this->ring_, amount - first);
      this->read_pos_ = (this->read_pos_ + amount) % RING_SIZE;
      this->used_ -= amount;
      xSemaphoreGive(this->ring_mutex_);
    }
    if (amount != 0) {
      last_audio_ms = millis();
      response_started = true;
      if (httpd_resp_send_chunk(request, reinterpret_cast<const char *>(chunk), amount) != ESP_OK) {
        send_ok = false;
        break;
      }
    } else {
      if (millis() - last_audio_ms >= AUDIO_WAIT_TIMEOUT_MS)
        break;
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }
  }
  if (!response_started) {
    httpd_resp_set_status(request, "503 Service Unavailable");
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    httpd_resp_send(request, "Microphone produced no audio", HTTPD_RESP_USE_STRLEN);
  } else if (send_ok && !this->abort_.load()) {
    httpd_resp_send_chunk(request, nullptr, 0);
  }
  // The HTTP server owns the descriptor. Shutdown requests a close without reusing or freeing its fd here.
  if (this->socket_fd_ >= 0)
    shutdown(this->socket_fd_, SHUT_RDWR);
  httpd_req_async_handler_complete(request);
  xSemaphoreTake(this->ring_mutex_, portMAX_DELAY);
  this->state_.store(StreamState::FINISHED);
  xSemaphoreGive(this->ring_mutex_);
  this->enable_loop_soon_any_context();
}

}  // namespace esphome::microphone_http_stream
