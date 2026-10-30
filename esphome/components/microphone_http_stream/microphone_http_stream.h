#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_server.h>

namespace esphome::web_server {
class WebServer;
}

namespace esphome::microphone_http_stream {

class MicrophoneHttpStream final : public Component, public AsyncWebHandler {
 public:
  /// @param stream_path "/audio/microphones/<stream_id>" string literal built by code generation
  MicrophoneHttpStream(microphone::MicrophoneSource *source, web_server_base::WebServerBase *base,
                       web_server::WebServer *server, const char *stream_path, uint8_t max_clients)
      : source_(source), base_(base), server_(server), stream_path_(stream_path), max_clients_(max_clients) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 2.0f; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  enum class StreamState : uint8_t {
    STREAM_STATE_IDLE,
    STREAM_STATE_PREPARING,
    STREAM_STATE_READY,
    STREAM_STATE_FINISHED,
  };
  enum class StopReason : uint8_t {
    STOP_REASON_NONE,
    STOP_REASON_BUFFER_FULL,
    STOP_REASON_INVALID_FRAME,
    STOP_REASON_PEER_CLOSED,
    STOP_REASON_SEND_TIMEOUT,
    STOP_REASON_SEND_FAILED,
    STOP_REASON_AUDIO_TIMEOUT,
  };

  // Keep in sync with STREAM_PATH_PREFIX in __init__.py
  static constexpr size_t STREAM_PATH_PREFIX_LEN = sizeof("/audio/microphones/") - 1;
  static constexpr size_t SINGLE_RING_SIZE = 16 * 1024;
  static constexpr size_t MULTI_RING_SIZE = 128 * 1024;
  static constexpr size_t MAX_CLIENTS = 3;
  static constexpr size_t SEND_CHUNK_SIZE = 2048;
  static constexpr uint32_t AUDIO_POLL_MS = 100;
  static constexpr uint32_t AUDIO_WAIT_TIMEOUT_MS = 5000;
  static constexpr uint32_t SEND_TIMEOUT_SEC = 2;
  static constexpr uint32_t SEND_TASK_STACK_SIZE = 3072;

  struct ClientSlot {
    MicrophoneHttpStream *owner{nullptr};
    std::unique_ptr<ring_buffer::RingBuffer> ring;
    TaskHandle_t task{nullptr};
    httpd_req_t *async_request{nullptr};
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
    std::atomic<uint32_t> peak_ring_used{0};
#endif
    std::atomic<StopReason> stop_reason{StopReason::STOP_REASON_NONE};
    std::atomic<StreamState> state{StreamState::STREAM_STATE_IDLE};
    std::atomic<uint8_t> abort{0};

    /// Record the first reason the stream stops; later reasons are ignored.
    void set_stop_reason(StopReason reason) {
      StopReason none = StopReason::STOP_REASON_NONE;
      this->stop_reason.compare_exchange_strong(none, reason);
    }
    void stop(StopReason reason) {
      this->set_stop_reason(reason);
      this->abort.store(1);
    }
  };

  static void send_task(void *arg);
  static const LogString *stop_reason_name(StopReason reason);
  void stream_to_client_(ClientSlot *client);
  void on_audio_(std::span<const uint8_t> data);
  void fail_request_(AsyncWebServerRequest *request, const char *message);
  void release_slot_(ClientSlot *client);
  void fail_setup_(const LogString *reason);
  void register_handler_();
  const char *stream_id_() const { return this->stream_path_.c_str() + STREAM_PATH_PREFIX_LEN; }
  size_t ring_size_() const { return this->max_clients_ == 1 ? SINGLE_RING_SIZE : MULTI_RING_SIZE; }
  unsigned client_index_(const ClientSlot *client) const {
    return static_cast<unsigned>(client - this->clients_.data());
  }

  microphone::MicrophoneSource *source_;
  web_server_base::WebServerBase *base_;
  web_server::WebServer *server_;
  StringRef stream_path_;
  uint8_t max_clients_;

  std::array<ClientSlot, MAX_CLIENTS> clients_{};
  bool streaming_active_{false};  // Main loop only
};

}  // namespace esphome::microphone_http_stream

#endif  // USE_ESP32
