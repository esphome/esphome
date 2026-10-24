#pragma once

#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

#include <atomic>
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
                       web_server_base::WebServerBase *base, const char *stream_id)
      : source_(source), microphone_(microphone), base_(base), stream_id_(stream_id) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 2.0f; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  enum class StreamState : uint8_t { IDLE, PREPARING, READY, FINISHED };

  static constexpr size_t RING_SIZE = 16 * 1024;
  static constexpr size_t SEND_CHUNK_SIZE = 1024;
  static constexpr uint32_t AUDIO_WAIT_TIMEOUT_MS = 5000;
  static constexpr uint32_t SEND_TASK_STACK_SIZE = 5120;

  static void send_task_(void *arg);
  void run_send_task_();
  void on_audio_(const std::vector<uint8_t> &data);
  void fail_request_(AsyncWebServerRequest *request, const char *message);

  microphone::MicrophoneSource *source_;
  microphone::Microphone *microphone_;
  web_server_base::WebServerBase *base_;
  const char *stream_id_;
  char stream_path_[64]{};
  char sample_rate_header_[16]{};
  uint32_t sample_rate_{0};

  SemaphoreHandle_t ring_mutex_{nullptr};
  uint8_t *ring_{nullptr};
  size_t read_pos_{0};
  size_t write_pos_{0};
  size_t used_{0};
  httpd_req_t *async_request_{nullptr};
  int socket_fd_{-1};
  TaskHandle_t send_task_handle_{nullptr};
  std::atomic<StreamState> state_{StreamState::IDLE};
  std::atomic<bool> abort_{false};
  bool capture_started_{false};  // Main loop only
};

}  // namespace esphome::microphone_http_stream
