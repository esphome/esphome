#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio_file/audio_file.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace esphome::audio_file {

enum class AudioFileDecodingState : uint8_t {
  START_TASK,
  DECODING,
  IDLE,
};

class AudioFileMediaSource : public Component, public media_source::MediaSource {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // MediaSource interface implementation
  bool play_uri(const std::string &uri) override;
  void handle_command(media_source::MediaSourceCommand command) override;
  bool can_handle(const std::string &uri) const override { return uri.starts_with("audio-file://"); }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

 protected:
  /// @brief Allocates stack buffer and creates the decode task. Returns true on success.
  bool create_decode_task_();
  /// @brief Deletes the decode task and deallocates the stack buffer.
  void delete_decode_task_();

  static void decode_task(void *params);

  audio::AudioFile *current_file_{nullptr};
  AudioFileDecodingState decoding_state_{AudioFileDecodingState::IDLE};
  EventGroupHandle_t event_group_{nullptr};
  TaskHandle_t decode_task_handle_{nullptr};
  StaticTask_t decode_task_stack_;
  StackType_t *decode_task_stack_buffer_{nullptr};

  bool task_stack_in_psram_{false};
};

}  // namespace esphome::audio_file

#endif  // USE_ESP32
