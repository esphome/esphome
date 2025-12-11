#pragma once

#include "esphome/components/audio/audio.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

namespace esphome {
namespace file {

enum class FileDecodingState : uint8_t {
  START_TASK,
  DECODING,
  IDLE,
};

struct NamedAudioFile {
  audio::AudioFile *file;
  std::string file_id;
};

// Forward declaration
class FileMediaSource;

/// @brief Context for a single pipeline's file playback
struct FileSourcePipeline {
  audio::AudioFile *current_file{nullptr};
  FileDecodingState decoding_state{FileDecodingState::IDLE};
  EventGroupHandle_t event_group{nullptr};
  QueueHandle_t controls_queue{nullptr};
  TaskHandle_t decode_task_handle{nullptr};
  StaticTask_t decode_task_stack;
  StackType_t *decode_task_stack_buffer{nullptr};
};

/// @brief Parameters passed to decode task
struct DecodeTaskParams {
  FileMediaSource *source;
  size_t pipeline;
};

class FileMediaSource : public Component, public media_source::MediaSource {
 public:
  void setup() override;
  void loop() override;

  // MediaSource interface implementation
  void init_pipelines(size_t pipeline_count) override;
  bool play_uri(const std::string &uri, size_t pipeline) override;
  void handle_command(media_source::MediaSourceCommand command, size_t pipeline) override;
  media_source::MediaSourceCapabilities get_capabilities() override;

  void add_file(audio::AudioFile *media_file, std::string file_id) {
    this->files_.push_back(NamedAudioFile{media_file, std::move(file_id)});
  }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

 protected:
  std::vector<NamedAudioFile> files_;
  FixedVector<FileSourcePipeline> file_pipelines_;
  bool task_stack_in_psram_{false};

  static void decode_task(void *params);
};

}  // namespace file
}  // namespace esphome
