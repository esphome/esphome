#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/core/component.h"
#include "esphome/core/static_task.h"
#include "esphome/core/ring_buffer.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace esphome::http_request {

enum class HTTPDecodingState : uint8_t {
  START_TASKS,
  DECODING,
  IDLE,
};

class HTTPRequestMediaSource : public Component,
                               public media_source::MediaSource,
                               public Parented<HttpRequestComponent> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // MediaSource interface implementation
  bool play_uri(const std::string &uri) override;
  void handle_command(media_source::MediaSourceCommand command) override;
  bool can_handle(const std::string &uri) const override {
    return uri.starts_with("http://") || uri.starts_with("https://");
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_buffer_size(size_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

 protected:
  friend void read_task(void *params);
  friend void decode_task(void *params);

  std::string current_uri_;
  audio::AudioFileType current_audio_file_type_{audio::AudioFileType::NONE};

  HTTPDecodingState decoding_state_{HTTPDecodingState::IDLE};
  EventGroupHandle_t event_group_{nullptr};

  std::weak_ptr<RingBuffer> raw_file_ring_buffer_;

  StaticTask read_task_;
  StaticTask decode_task_;

  size_t buffer_size_{24 * 1024};  // Ring buffer size between read and decode tasks

  bool task_stack_in_psram_{false};
};

}  // namespace esphome::http_request

#endif  // USE_ESP32
