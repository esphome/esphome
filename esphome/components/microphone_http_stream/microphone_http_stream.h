#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_idf/sse_chunk.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace esphome::microphone_http_stream {

// Keep in sync with STREAM_PATH_PREFIX in __init__.py
static constexpr char STREAM_PATH_PREFIX[] = "/audio/microphones/";
static constexpr size_t STREAM_PATH_PREFIX_LEN = sizeof(STREAM_PATH_PREFIX) - 1;
// The prefix without its trailing slash
static constexpr StringRef LIST_PATH(STREAM_PATH_PREFIX, STREAM_PATH_PREFIX_LEN - 1);

enum class StreamState : uint8_t {
  STREAM_STATE_IDLE,
  STREAM_STATE_PREPARING,
  STREAM_STATE_READY,
  STREAM_STATE_FINISHING,  // Stream deleted; waits for in-flight audio writes before becoming IDLE
};

// Keep in sync with StopReasonStrings in microphone_http_stream.cpp
enum class StopReason : uint8_t {
  STOP_REASON_NONE,
  STOP_REASON_BUFFER_FULL,
  STOP_REASON_PEER_CLOSED,
  STOP_REASON_SEND_FAILED,
  STOP_REASON_AUDIO_TIMEOUT,
};

struct ClientSlot {
  std::unique_ptr<ring_buffer::RingBuffer> ring;
  AsyncStreamResponse *stream{nullptr};  // Published by the httpd task before state becomes READY
  // HTTP chunk being sent, main loop only. The ring item stays acquired until the whole chunk is out.
  void *chunk_data{nullptr};
  uint32_t last_audio_ms{0};  // Main loop time; 0 until the first send pass starts the audio timeout
  uint16_t chunk_len{0};
  uint16_t chunk_sent{0};  // Bytes of chunk_header + data + CHUNK_END already accepted by the socket
  char chunk_header[web_server_idf::CHUNK_HDR_LEN];
  std::atomic<StopReason> stop_reason{StopReason::STOP_REASON_NONE};  // Not NONE once the stream must stop
  std::atomic<StreamState> state{StreamState::STREAM_STATE_IDLE};
  std::atomic<uint8_t> writers{0};  // Audio callbacks currently using this slot

  /// Record the first reason the stream stops; later reasons are ignored.
  void set_stop_reason(StopReason reason) {
    StopReason none = StopReason::STOP_REASON_NONE;
    this->stop_reason.compare_exchange_strong(none, reason);
  }
};

class MicrophoneHttpStream final : public Component, public AsyncWebHandler {
 public:
  /// @param stream_path "/audio/microphones/<stream_id>" string literal built by code generation
  /// @param clients one slot per allowed client, static storage built by code generation
  MicrophoneHttpStream(microphone::MicrophoneSource *source, web_server_base::WebServerBase *base,
                       const char *stream_path, std::span<ClientSlot> clients)
      : source_(source), base_(base), stream_path_(stream_path), clients_(clients) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 2.0f; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  static constexpr size_t SINGLE_RING_SIZE = 16 * 1024;
  static constexpr size_t MULTI_RING_SIZE = 128 * 1024;
  static constexpr size_t SEND_CHUNK_SIZE = 2048;
  static constexpr uint32_t AUDIO_WAIT_TIMEOUT_MS = 5000;

  void service_client_(ClientSlot *client);
  void send_audio_(ClientSlot *client);
  void finish_client_(ClientSlot *client);
  void on_audio_(std::span<const uint8_t> data);
  void write_audio_(ClientSlot *client, std::span<const uint8_t> data);
  const char *stream_id_() const { return this->stream_path_ + STREAM_PATH_PREFIX_LEN; }
  unsigned client_index_(const ClientSlot *client) const {
    return static_cast<unsigned>(client - this->clients_.data());
  }

  microphone::MicrophoneSource *source_;
  web_server_base::WebServerBase *base_;
  const char *stream_path_;
  std::span<ClientSlot> clients_;
  bool large_ring_{false};  // Set in setup()
};

}  // namespace esphome::microphone_http_stream

#endif  // USE_ESP32
