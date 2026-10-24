#pragma once

#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_http_server.h>

namespace esphome::microphone_http_stream {

class MicrophoneHttpStream final : public Component, public AsyncWebHandler {
 public:
  MicrophoneHttpStream(microphone::MicrophoneSource *source, microphone::Microphone *microphone,
                       web_server_base::WebServerBase *base, const char *stream_id, uint8_t max_clients)
      : source_(source),
        microphone_(microphone),
        base_(base),
        stream_id_(stream_id),
        max_clients_(max_clients),
        ring_size_(max_clients == 1 ? SINGLE_RING_SIZE : MULTI_RING_SIZE) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 2.0f; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  enum class StreamState : uint8_t { IDLE, PREPARING, READY, FINISHED };
  enum class StopReason : uint8_t {
    STOP_REASON_NONE,
    STOP_REASON_BUFFER_FULL,
    STOP_REASON_INVALID_FRAME,
    STOP_REASON_OVERLAPPING_AUDIO,
    STOP_REASON_SOURCE_BUSY,
    STOP_REASON_PEER_CLOSED,
    STOP_REASON_SEND_TIMEOUT,
    STOP_REASON_SEND_FAILED,
    STOP_REASON_AUDIO_TIMEOUT,
  };

  static constexpr size_t SINGLE_RING_SIZE = 16 * 1024;
  static constexpr size_t MULTI_RING_SIZE = 128 * 1024;
  static constexpr size_t MAX_CLIENTS = 3;
  static constexpr size_t SEND_CHUNK_SIZE = 2048;
  static constexpr uint32_t AUDIO_WAIT_TIMEOUT_MS = 5000;
  static constexpr uint32_t SEND_TIMEOUT_SEC = 2;
  static constexpr uint32_t SEND_TASK_STACK_SIZE = 5120;

  struct ClientSlot {
    MicrophoneHttpStream *owner{nullptr};
    uint8_t index{0};
    SemaphoreHandle_t data_ready{nullptr};
    uint8_t *ring{nullptr};
    std::atomic<size_t> read_seq{0};
    std::atomic<size_t> write_seq{0};
    std::atomic<uint32_t> generation{0};
    std::atomic<bool> producer_active{false};
    std::atomic<uint32_t> peak_ring_used{0};
    std::atomic<uint32_t> max_send_ms{0};
    std::atomic<StopReason> stop_reason{StopReason::STOP_REASON_NONE};
    httpd_req_t *async_request{nullptr};
    int socket_fd{-1};
    std::atomic<StreamState> state{StreamState::IDLE};
    std::atomic<bool> abort{false};
  };

  static void send_task_(void *arg);
  void run_send_task_(ClientSlot *client);
  void on_audio_(const std::vector<uint8_t> &data);
  void fail_request_(AsyncWebServerRequest *request, const char *message);

  microphone::MicrophoneSource *source_;
  microphone::Microphone *microphone_;
  web_server_base::WebServerBase *base_;
  const char *stream_id_;
  uint8_t max_clients_;
  size_t ring_size_;
  char stream_path_[64]{};
  char sample_rate_header_[16]{};
  uint32_t sample_rate_{0};

  SemaphoreHandle_t control_mutex_{nullptr};
  std::array<ClientSlot, MAX_CLIENTS> clients_{};
  bool capture_started_{false};  // Main loop only
};

}  // namespace esphome::microphone_http_stream
