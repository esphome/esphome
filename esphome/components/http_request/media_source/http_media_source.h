#pragma once

#include "esphome/components/audio/audio.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

#include "../http_request.h"

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

namespace esphome {
namespace http_request {

enum class HTTPDecodingState : uint8_t {
  START_TASKS,
  DECODING,
  IDLE,
};

// Forward declaration
class HTTPMediaSource;

/// @brief Context for a single pipeline's HTTP streaming playback
struct HTTPSourcePipeline {
  std::string current_uri;
  audio::AudioFileType current_audio_file_type{audio::AudioFileType::NONE};
  HTTPDecodingState decoding_state{HTTPDecodingState::IDLE};
  EventGroupHandle_t event_group{nullptr};
  QueueHandle_t controls_queue{nullptr};
  std::weak_ptr<RingBuffer> raw_file_ring_buffer;
  TaskHandle_t read_task_handle{nullptr};
  StaticTask_t read_task_stack;
  StackType_t *read_task_stack_buffer{nullptr};
  TaskHandle_t decode_task_handle{nullptr};
  StaticTask_t decode_task_stack;
  StackType_t *decode_task_stack_buffer{nullptr};
};

/// @brief Parameters passed to read and decode tasks
struct HTTPTaskParams {
  HTTPMediaSource *source;
  size_t pipeline;
};

class HTTPMediaSource : public Component, public media_source::MediaSource, public Parented<HttpRequestComponent> {
 public:
  void setup() override;
  void loop() override;

  // MediaSource interface implementation
  void init_pipelines(size_t pipeline_count) override;
  bool play_uri(const std::string &uri, size_t pipeline) override;
  void handle_command(media_source::MediaSourceCommand command, size_t pipeline) override;
  media_source::MediaSourceCapabilities get_capabilities() override;

  void set_buffer_size(size_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

 protected:
  size_t buffer_size_{24 * 1024};  // Ring buffer size between read and decode tasks
  FixedVector<HTTPSourcePipeline> http_pipelines_;
  bool task_stack_in_psram_{false};

  static void read_task(void *params);
  static void decode_task(void *params);
};

}  // namespace http_request
}  // namespace esphome
